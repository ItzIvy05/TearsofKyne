#include <optional>

#include "Events.h"
#include "Hotkeys.h"
#include "Localization.h"
#include "Manager.h"
#include "Menu.h"
#include "Serialization.h"
#include "Settings.h"
#include "UI.h"
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

        if (!TearsWidget::IsInGameSession()) {
            return;
        }

        if (!PlayerReadyForSurvivalSync()) {
            return;
        }

        g_pendingNewGameSurvivalSync.store(false);

        bool enableWaterNeeds;
        if (Settings::g_enableTearsWithSM) {
            enableWaterNeeds = GetSurvivalModeToggleState().value_or(true);
        } else {
            enableWaterNeeds = Settings::g_enableTears;
        }

        WaterskinUtils::SetSystemEnabled(enableWaterNeeds);

        if (enableWaterNeeds) {
            WaterskinUtils::QueueStartingWaterskin();
        } else {
            WaterskinUtils::CancelPendingStartingWaterskin();
        }

        logger::info("[Tears of Kyne] New game startup sync applied. Water needs {}.",
                     enableWaterNeeds ? "enabled" : "disabled");
    }

    void ReconcileSystemEnabledState() {
        if (!TearsWidget::IsInGameSession()) {
            return;
        }

        bool shouldEnable;
        if (Settings::g_enableTearsWithSM) {
            const auto smState = GetSurvivalModeToggleState();
            if (!smState.has_value()) {
                return;
            }
            shouldEnable = Settings::g_enableTears && *smState;
        } else {
            shouldEnable = Settings::g_enableTears;
        }

        if (WaterNeedManager::GetSingleton()->IsSystemEnabled() != shouldEnable) {
            WaterskinUtils::SetSystemEnabled(shouldEnable);
            if (shouldEnable) {
                WaterskinUtils::QueueStartingWaterskin();
            }
        }
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
        if (TearsWidget::IsInGameSession()) {
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

        TearsWidget::BeginGameSessionTransition(false);
        WaterNeedManager::GetSingleton()->OnGameLoaded();
        if (!Serialization::HasLoadedSaveData()) {
            InitializeFreshInstallState();
        }
        TearsWidget::Refresh();
        g_runtimeBootstrapHandled.store(true);
        logger::info("[Tears of Kyne] In-world startup fallback applied.");
    }

    void StartUpdateThread() {
        if (g_updateThreadStarted.exchange(true)) {
            return;
        }

        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            int elapsed = 0;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                elapsed += 1;
                const bool slowTick = (elapsed >= Settings::g_updateIntervalSec);
                if (slowTick) elapsed = 0;

                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([slowTick] {
                        if (Settings::g_perkFormsDirty.exchange(false)) {
                            Settings::ParsePerkForms();
                        }
                        if (slowTick) {
                            BootstrapInWorldStartupIfNeeded();
                            TearsWidget::RefreshSuppression();
                            ApplyPendingNewGameSurvivalSyncIfReady();
                            WaterNeedManager::GetSingleton()->Tick();
                        }
                        ReconcileSystemEnabledState();
                        TearsWidget::ApplyPendingAppearance();
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
                Localization::Load();
                Serialization::ResetLoadState();
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(false);

                WaterskinUtils::Initialize();
                WaterNeedManager::GetSingleton()->InitializeFromSettings();
                TearsWidget::Init();
                TearsWidget::EndGameSession();
                Events::RegisterAll();
                Hotkeys::Initialize();
                UI::Register();
                StartUpdateThread();

                logger::info("[Tears of Kyne] Ready. FillKey={} ToggleWidgetKey={} HudX={} HudY={}",
                             Settings::GetKeyName(Settings::g_fillKey),
                             Settings::GetKeyName(Settings::g_toggleWidgetKey), Settings::g_hudX, Settings::g_hudY);
                break;
            }
            case SKSE::MessagingInterface::kNewGame:
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(true);
                TearsWidget::BeginGameSessionTransition(true);
                WaterNeedManager::GetSingleton()->OnNewGame();
                WaterskinUtils::CancelPendingStartingWaterskin();
                TearsWidget::Refresh();
                break;
            case SKSE::MessagingInterface::kPostLoadGame:
                g_runtimeBootstrapHandled.store(false);
                g_pendingNewGameSurvivalSync.store(false);
                TearsWidget::BeginGameSessionTransition(false);
                WaterNeedManager::GetSingleton()->OnGameLoaded();
                if (!Serialization::HasLoadedSaveData()) {
                    InitializeFreshInstallState();
                }
                TearsWidget::Refresh();
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