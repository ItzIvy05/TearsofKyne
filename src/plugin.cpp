#include <optional>

#include "Events.h"
#include "Hooks.h"
#include "Hotkeys.h"
#include "MCP.h"
#include "Manager.h"
#include "Serialization.h"
#include "Settings.h"
#include "Utils.h"
#include "logger.h"

namespace {
    std::atomic_bool g_updateThreadStarted = false;
    std::atomic_bool g_runtimeBootstrapHandled = false;
    std::atomic_bool g_pendingNewGameSurvivalSync = false;

    RE::TESGlobal* LookupSurvivalModeGlobal(const char* editorID) {
        return RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
    }

    std::optional<bool> GetSurvivalModeToggleState() {
        auto* survivalToggle = LookupSurvivalModeGlobal("Survival_ModeToggle");
        if (!survivalToggle) {
            logger::info(
                "[Tears of Kyne] Survival_ModeToggle not found. New game sync will keep Tears of Kyne enabled.");
            return std::nullopt;
        }

        return survivalToggle->value > 0.0f;
    }

    bool PlayerReadyForSurvivalSync() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (!player || !ui) {
            return false;
        }

        if (!player->Is3DLoaded() || !player->GetParentCell()) {
            return false;
        }

        if (ui->IsMenuOpen("Main Menu") || ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("RaceSex Menu")) {
            return false;
        }

        return true;
    }

    void ApplyPendingNewGameSurvivalSyncIfReady() {
        if (!g_pendingNewGameSurvivalSync.load()) {
            return;
        }

        if (!HUDManager::GetSingleton()->IsInGameSession()) {
            return;
        }

        if (!PlayerReadyForSurvivalSync()) {
            return;
        }

        g_pendingNewGameSurvivalSync.store(false);

        const auto survivalToggle = GetSurvivalModeToggleState();
        const bool enableWaterNeeds = survivalToggle.value_or(true);

        WaterskinUtils::SetSystemEnabled(enableWaterNeeds);

        if (enableWaterNeeds) {
            WaterskinUtils::QueueStartingWaterskin();
        } else {
            WaterskinUtils::CancelPendingStartingWaterskin();
        }

        logger::info("[Tears of Kyne] New game Survival Mode sync applied. Water needs {}.",
                     enableWaterNeeds ? "enabled" : "disabled");
    }

    void InitializeFreshInstallState() {
        auto* manager = WaterNeedManager::GetSingleton();
        manager->ForceSet(0.0f);
        manager->SetSystemEnabled(true);
        manager->ClearStoredBottleCounts();
        WaterskinUtils::QueueStartingWaterskin();

        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyEnableMod")) {
            global->value = 1.0f;
        }

        logger::info("[Tears of Kyne] No existing save data found. Queued first-time startup bottle.");
    }

    void BootstrapInWorldStartupIfNeeded() {
        auto* hud = HUDManager::GetSingleton();
        if (hud->IsInGameSession()) {
            g_runtimeBootstrapHandled.store(true);
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (!player || !ui) {
            return;
        }

        if (!player->Is3DLoaded() || !player->GetParentCell()) {
            return;
        }

        if (ui->IsMenuOpen("Main Menu") || ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("RaceSex Menu")) {
            return;
        }

        if (g_runtimeBootstrapHandled.load()) {
            return;
        }

        hud->BeginGameSessionTransition(false);
        WaterNeedManager::GetSingleton()->OnGameLoaded();
        if (!Serialization::HasLoadedSaveData()) {
            InitializeFreshInstallState();
        }
        hud->PushUpdate();
        g_runtimeBootstrapHandled.store(true);
        logger::info("[Tears of Kyne] In-world startup fallback applied.");
    }

    void StartUpdateThread() {
        if (g_updateThreadStarted.exchange(true)) {
            return;
        }

        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(Settings::g_updateIntervalSec));
                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([] {
                        BootstrapInWorldStartupIfNeeded();
                        HUDManager::GetSingleton()->RefreshMenuSuppression();
                        ApplyPendingNewGameSurvivalSyncIfReady();
                        WaterNeedManager::GetSingleton()->Tick();
                    });
                }
            }
        }).detach();

        logger::info("[Tears of Kyne] Update thread started ({}s).", Settings::g_updateIntervalSec);
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        switch (message->type) {
            case SKSE::MessagingInterface::kDataLoaded: {
                Settings::LoadFromINI();
                Serialization::ResetLoadState();
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(false);

                auto* prismaUI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
                    PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));

                WaterskinUtils::Initialize();
                WaterNeedManager::GetSingleton()->InitializeFromSettings();
                HUDManager::GetSingleton()->Initialize(prismaUI);
                HUDManager::GetSingleton()->EndGameSession();
                Events::RegisterAll();
                Hooks::Install();
                Hotkeys::Initialize();
                StartUpdateThread();

                logger::info("[Tears of Kyne] Ready. FillKey={} MenuKey={} HudX={} HudY={} ToggleHUDKey={}",
                             Settings::GetKeyName(Settings::g_fillKey), Settings::GetKeyName(Settings::g_menuKey),
                             Settings::g_hudX, Settings::g_hudY, Settings::GetKeyName(Settings::g_toggleHUDKey));
                break;
            }
            case SKSE::MessagingInterface::kNewGame:
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(true);
                HUDManager::GetSingleton()->BeginGameSessionTransition(true);
                WaterNeedManager::GetSingleton()->OnNewGame();
                WaterskinUtils::CancelPendingStartingWaterskin();
                HUDManager::GetSingleton()->PushUpdate();
                break;
            case SKSE::MessagingInterface::kPostLoadGame:
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(false);
                HUDManager::GetSingleton()->BeginGameSessionTransition(false);
                WaterNeedManager::GetSingleton()->OnGameLoaded();
                if (!Serialization::HasLoadedSaveData()) {
                    InitializeFreshInstallState();
                }
                HUDManager::GetSingleton()->PushUpdate();
                break;
            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLog();
    REL::Module::reset();

    logger::info("[Tears of Kyne] Plugin loaded.");

    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    auto* serialization = SKSE::GetSerializationInterface();
    if (serialization) {
        serialization->SetUniqueID(Settings::SERIAL_UID);
        serialization->SetSaveCallback(Serialization::OnSave);
        serialization->SetLoadCallback(Serialization::OnLoad);
        serialization->SetRevertCallback(Serialization::OnRevert);
    }

    return true;
}