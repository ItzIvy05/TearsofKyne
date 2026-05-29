#include "Utils.h"

#include <optional>

#include "MCP.h"
#include "Manager.h"

namespace WaterskinUtils {
    namespace {
        constexpr std::array<RE::FormID, 4> TRACKED_BOTTLE_FORM_IDS = {0x00000800, 0x00000802, 0x00000803, 0x00000804};

        constexpr std::array<const char*, 4> TRACKED_BOTTLE_EDITOR_IDS = {"IvyWaterBottle01", "IvyWaterBottle02",
                                                                          "IvyWaterBottle03", "IvyWaterBottle"};

        constexpr const char* FILLED_WATERSKIN_EDITOR_ID = "IvyWaterBottle01";
        constexpr const char* EMPTY_WATERSKIN_EDITOR_ID = "IvyWaterBottle";
        constexpr const char* REFILL_ACTIVATOR_KEYWORD_EDITOR_ID = "IvyRefillActivator";
        constexpr const char* HYDRATING_DRINK_KEYWORD_EDITOR_ID = "IvyHydratingDrinkKeyword";
        constexpr float HYDRATING_DRINK_AMOUNT = 15.0f;

        RE::TESBoundObject* g_emptyWaterskin = nullptr;
        RE::TESBoundObject* g_filledWaterskin = nullptr;
        std::array<RE::TESBoundObject*, 4> g_trackedBottleForms{};
        bool g_pendingStartingBottleGrant = false;
        bool g_loggedLookupFailure = false;

        float GetCurrentGameTime() {
            auto* calendar = RE::Calendar::GetSingleton();
            return calendar ? calendar->GetCurrentGameTime() : -1.0f;
        }

        bool EnsureWaterskinFormsReady(bool logFailure = true);

        bool IsAlreadyQuenched() { return WaterNeedManager::GetSingleton()->GetStage() == 0; }

        std::string GetCurrentStateNotificationText() {
            const auto* manager = WaterNeedManager::GetSingleton();
            const auto* stageName = manager ? manager->GetStageName() : "Quenched";
            return std::format("I'm {}.", stageName ? stageName : "Quenched");
        }

        bool HasAnyTrackedBottle(RE::PlayerCharacter* player) {
            if (!player) {
                return false;
            }

            for (auto* form : g_trackedBottleForms) {
                if (form && player->GetItemCount(form) > 0) {
                    return true;
                }
            }

            return false;
        }

        std::optional<std::size_t> FindDrinkableBottleIndex(RE::PlayerCharacter* player) {
            if (!player) {
                return std::nullopt;
            }

            for (std::size_t i = 0; i < 3; ++i) {
                auto* form = g_trackedBottleForms[i];
                if (form && player->GetItemCount(form) > 0) {
                    return i;
                }
            }

            return std::nullopt;
        }

        std::optional<std::size_t> FindRefillableBottleIndex(RE::PlayerCharacter* player) {
            if (!player) {
                return std::nullopt;
            }

            for (std::size_t i = g_trackedBottleForms.size(); i > 1; --i) {
                auto* form = g_trackedBottleForms[i - 1];
                if (form && player->GetItemCount(form) > 0) {
                    return i - 1;
                }
            }

            return std::nullopt;
        }

        std::optional<std::size_t> FindTrackedBottleIndexByFormID(RE::FormID formID) {
            for (std::size_t i = 0; i < g_trackedBottleForms.size(); ++i) {
                auto* form = g_trackedBottleForms[i];
                if (form && form->GetFormID() == formID) {
                    return i;
                }
            }

            return std::nullopt;
        }

        bool ReplaceTrackedBottle(RE::PlayerCharacter* player, std::size_t fromIndex, std::size_t toIndex) {
            if (!player || fromIndex >= g_trackedBottleForms.size() || toIndex >= g_trackedBottleForms.size()) {
                return false;
            }

            auto* fromBottle = g_trackedBottleForms[fromIndex];
            auto* toBottle = g_trackedBottleForms[toIndex];
            if (!fromBottle || !toBottle || player->GetItemCount(fromBottle) <= 0) {
                return false;
            }

            player->RemoveItem(fromBottle, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            player->AddObjectToContainer(toBottle, nullptr, 1, nullptr);
            return true;
        }

        bool IsRefillActivatorBase(RE::TESBoundObject* base) {
            return base && base->As<RE::TESObjectACTI>() &&
                   base->HasKeywordByEditorID(REFILL_ACTIVATOR_KEYWORD_EDITOR_ID);
        }

        bool IsHydratingDrink(RE::FormID baseObjectFormID) {
            auto* form = RE::TESForm::LookupByID(baseObjectFormID);
            if (!form) {
                return false;
            }

            auto* boundObject = form->As<RE::TESBoundObject>();
            if (!boundObject || !boundObject->As<RE::AlchemyItem>()) {
                return false;
            }

            return boundObject->HasKeywordByEditorID(HYDRATING_DRINK_KEYWORD_EDITOR_ID);
        }

        void TryDrinkFromHydratingDrink(RE::FormID baseObjectFormID) {
            if (!IsHydratingDrink(baseObjectFormID)) {
                return;
            }

            if (IsAlreadyQuenched()) {
                HUDManager::GetSingleton()->ShowNotification("I'm already Quenched.");
                HUDManager::GetSingleton()->PushUpdate();
                return;
            }

            WaterNeedManager::GetSingleton()->Drink(HYDRATING_DRINK_AMOUNT);

            const auto currentStateText = GetCurrentStateNotificationText();
            HUDManager::GetSingleton()->ShowNotification(currentStateText.c_str());
            HUDManager::GetSingleton()->PushUpdate();
        }

        bool TryFillImpl(bool requireNearbyWater) {
            if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
                return false;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !EnsureWaterskinFormsReady()) {
                return false;
            }

            if (requireNearbyWater && !IsNearWater()) {
                HUDManager::GetSingleton()->ShowNotification("I need to stand in water to fill my waterskin.");
                return false;
            }

            const auto refillableBottleIndex = FindRefillableBottleIndex(player);
            if (refillableBottleIndex.has_value()) {
                ReplaceTrackedBottle(player, *refillableBottleIndex, 0);
            }

            if (!IsAlreadyQuenched()) {
                WaterNeedManager::GetSingleton()->ForceSet(0.0f);
            }

            const auto currentStateText = GetCurrentStateNotificationText();
            HUDManager::GetSingleton()->ShowNotification(currentStateText.c_str());
            HUDManager::GetSingleton()->PushUpdate();
            return true;
        }

        RE::PlayerCharacter* GetReadyPlayer() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded() || !player->GetParentCell()) {
                return nullptr;
            }
            return player;
        }

        RE::TESBoundObject* LookupBoundObjectByEditorID(const char* editorID) {
            if (!editorID || !*editorID) {
                return nullptr;
            }

            auto* form = RE::TESForm::LookupByEditorID(editorID);
            return form ? static_cast<RE::TESBoundObject*>(form) : nullptr;
        }

        RE::TESBoundObject* LookupBoundObjectWithFallback(RE::TESDataHandler* dataHandler, RE::FormID formID,
                                                          const char* editorID) {
            if (dataHandler) {
                if (auto* form = dataHandler->LookupForm<RE::TESBoundObject>(formID, Settings::g_pluginName.c_str())) {
                    return form;
                }
            }

            return LookupBoundObjectByEditorID(editorID);
        }

        bool ResolveWaterskinForms(bool logSuccess, bool logFailure) {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                if (logFailure && !g_loggedLookupFailure) {
                    logger::warn("[WaterskinUtils] TESDataHandler unavailable while resolving waterskin forms.");
                    g_loggedLookupFailure = true;
                }
                return false;
            }

            g_filledWaterskin = LookupBoundObjectWithFallback(dataHandler, Settings::g_filledWaterskinFormID,
                                                              FILLED_WATERSKIN_EDITOR_ID);
            g_emptyWaterskin =
                LookupBoundObjectWithFallback(dataHandler, Settings::g_emptyWaterskinFormID, EMPTY_WATERSKIN_EDITOR_ID);

            bool allTrackedResolved = true;
            for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                g_trackedBottleForms[i] = LookupBoundObjectWithFallback(dataHandler, TRACKED_BOTTLE_FORM_IDS[i],
                                                                        TRACKED_BOTTLE_EDITOR_IDS[i]);
                if (!g_trackedBottleForms[i]) {
                    allTrackedResolved = false;
                }
            }

            const bool resolved = g_filledWaterskin != nullptr && g_emptyWaterskin != nullptr && allTrackedResolved;

            if (resolved) {
                if (logSuccess) {
                    logger::info("[WaterskinUtils] Filled waterskin '{}'.", g_filledWaterskin->GetName());
                    logger::info("[WaterskinUtils] Empty waterskin '{}'.", g_emptyWaterskin->GetName());
                    for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                        logger::info("[WaterskinUtils] Tracked bottle '{}' (EditorID '{}').",
                                     g_trackedBottleForms[i]->GetName(), TRACKED_BOTTLE_EDITOR_IDS[i]);
                    }
                }
                g_loggedLookupFailure = false;
                return true;
            }

            if (logFailure && !g_loggedLookupFailure) {
                if (!g_filledWaterskin) {
                    logger::warn("[WaterskinUtils] Filled waterskin 0x{:X} / EditorID '{}' not resolved yet.",
                                 Settings::g_filledWaterskinFormID, FILLED_WATERSKIN_EDITOR_ID);
                }
                if (!g_emptyWaterskin) {
                    logger::warn("[WaterskinUtils] Empty waterskin 0x{:X} / EditorID '{}' not resolved yet.",
                                 Settings::g_emptyWaterskinFormID, EMPTY_WATERSKIN_EDITOR_ID);
                }
                for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                    if (!g_trackedBottleForms[i]) {
                        logger::warn("[WaterskinUtils] Tracked bottle 0x{:X} / EditorID '{}' not resolved yet.",
                                     TRACKED_BOTTLE_FORM_IDS[i], TRACKED_BOTTLE_EDITOR_IDS[i]);
                    }
                }
                g_loggedLookupFailure = true;
            }

            return false;
        }

        bool EnsureWaterskinFormsReady(bool logFailure) {
            if (g_filledWaterskin && g_emptyWaterskin) {
                bool allTrackedResolved = true;
                for (auto* form : g_trackedBottleForms) {
                    if (!form) {
                        allTrackedResolved = false;
                        break;
                    }
                }
                if (allTrackedResolved) {
                    return true;
                }
            }

            return ResolveWaterskinForms(false, logFailure);
        }

        bool AddTrackedBottle(RE::PlayerCharacter* player, RE::TESBoundObject* bottle, std::int32_t count) {
            if (!player || !bottle || count <= 0) {
                return false;
            }

            player->AddObjectToContainer(bottle, nullptr, count, nullptr);
            return true;
        }

        void RefreshInventoryMenuNow() {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                return;
            }

            auto inventoryMenu = ui->GetMenu<RE::InventoryMenu>();
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (inventoryMenu && player) {
                auto& runtimeData = inventoryMenu->GetRuntimeData();
                if (runtimeData.itemList) {
                    runtimeData.itemList->Update(player);
                }
            }

            if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                queue->AddMessage(RE::BSFixedString(RE::InventoryMenu::MENU_NAME.data()),
                                  RE::UI_MESSAGE_TYPE::kInventoryUpdate, nullptr);
            }
        }

        void QueueInventoryMenuRefresh() {
            SKSE::GetTaskInterface()->AddTask([]() { RefreshInventoryMenuNow(); });
        }

        void RestoreStoredOrGiveDefaultBottle() {
            auto* manager = WaterNeedManager::GetSingleton();
            if (!manager->IsSystemEnabled()) {
                return;
            }

            if (!EnsureWaterskinFormsReady()) {
                g_pendingStartingBottleGrant = true;
                return;
            }

            auto* player = GetReadyPlayer();
            if (!player) {
                g_pendingStartingBottleGrant = true;
                return;
            }

            const auto storedBottleCounts = manager->GetStoredBottleCounts();
            std::int32_t totalRequestedRestore = 0;
            std::int32_t totalVerifiedRestore = 0;
            for (std::size_t i = 0; i < g_trackedBottleForms.size(); ++i) {
                const auto count = storedBottleCounts[i];
                if (count > 0 && g_trackedBottleForms[i]) {
                    totalRequestedRestore += count;
                    if (AddTrackedBottle(player, g_trackedBottleForms[i], count)) {
                        totalVerifiedRestore += count;
                    }
                }
            }

            if (totalRequestedRestore > 0 && totalVerifiedRestore < totalRequestedRestore) {
                logger::warn("[WaterskinUtils] Some stored bottles could not be restored yet.");
            }

            if (totalRequestedRestore > 0) {
                g_pendingStartingBottleGrant = false;
                manager->ClearStoredBottleCounts();
            }

            if (!HasAnyTrackedBottle(player)) {
                auto* startingBottle = g_trackedBottleForms[0] ? g_trackedBottleForms[0] : g_filledWaterskin;
                if (!startingBottle || !AddTrackedBottle(player, startingBottle, 1)) {
                    g_pendingStartingBottleGrant = true;
                    logger::warn(
                        "[WaterskinUtils] Deferred starting bottle grant because inventory was not ready yet.");
                    return;
                }

                HUDManager::GetSingleton()->ShowNotification("I received a water bottle.");
            }

            g_pendingStartingBottleGrant = false;
            manager->ClearStoredBottleCounts();
            const auto now = GetCurrentGameTime();
            if (now >= 0.0f) {
                manager->SetLastGameTime(now);
            }
            HUDManager::GetSingleton()->PushUpdate();
        }
    }

    void Initialize() {
        g_filledWaterskin = nullptr;
        g_emptyWaterskin = nullptr;
        g_trackedBottleForms.fill(nullptr);
        g_pendingStartingBottleGrant = false;
        g_loggedLookupFailure = false;
        ResolveWaterskinForms(true, false);
    }

    void TryFill() { TryFillImpl(true); }

    bool TryFillFromActivator(RE::TESObjectREFR* activatedRef, RE::TESObjectREFR* actionRef) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || actionRef != player || !activatedRef) {
            return false;
        }

        auto* base = activatedRef->GetBaseObject();
        if (!IsRefillActivatorBase(base)) {
            return false;
        }

        return TryFillImpl(false);
    }

    void TryDrink() {
        if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !EnsureWaterskinFormsReady()) {
            return;
        }

        const auto drinkableBottleIndex = FindDrinkableBottleIndex(player);
        if (!drinkableBottleIndex.has_value()) {
            if (g_trackedBottleForms[3] && player->GetItemCount(g_trackedBottleForms[3]) > 0) {
                HUDManager::GetSingleton()->ShowNotification("My waterskin is empty.");
            } else {
                HUDManager::GetSingleton()->ShowNotification("I do not have a filled waterskin.");
            }
            return;
        }

        if (IsAlreadyQuenched()) {
            HUDManager::GetSingleton()->ShowNotification("I'm already Quenched.");
            return;
        }

        if (!ReplaceTrackedBottle(player, *drinkableBottleIndex, *drinkableBottleIndex + 1)) {
            HUDManager::GetSingleton()->ShowNotification("I could not drink from my waterskin.");
            return;
        }

        WaterNeedManager::GetSingleton()->Drink(Settings::g_drinkAmount);
        QueueInventoryMenuRefresh();
        const auto currentStateText = GetCurrentStateNotificationText();
        HUDManager::GetSingleton()->ShowNotification(currentStateText.c_str());
        HUDManager::GetSingleton()->PushUpdate();
    }

    bool IsNearWater() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        if (player->IsInWater()) {
            return true;
        }

        auto* cell = player->GetParentCell();
        if (!cell) {
            return false;
        }

        bool found = false;
        cell->ForEachReferenceInRange(
            player->GetPosition(), Settings::g_waterScanRadius, [&](RE::TESObjectREFR* reference) {
                if (!reference) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                auto* base = reference->GetBaseObject();
                if (!base) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (base->GetFormType() == RE::FormType::Water) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                if (IsRefillActivatorBase(base)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto* name = base->GetName();
                if (!name || !*name) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                std::string_view text(name);
                if (text.find("Water") != std::string_view::npos || text.find("River") != std::string_view::npos ||
                    text.find("Stream") != std::string_view::npos || text.find("Well") != std::string_view::npos ||
                    text.find("Lake") != std::string_view::npos || text.find("Pond") != std::string_view::npos) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

        return found;
    }

    bool IsRefillActivator(RE::TESObjectREFR* reference) {
        return reference && IsRefillActivatorBase(reference->GetBaseObject());
    }

    bool HasEmptyWaterskin() {
        if (!EnsureWaterskinFormsReady(false)) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && FindRefillableBottleIndex(player).has_value();
    }

    bool HasFilledWaterskin() {
        if (!EnsureWaterskinFormsReady(false)) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && FindDrinkableBottleIndex(player).has_value();
    }

    void GiveStartingWaterskin() { RestoreStoredOrGiveDefaultBottle(); }

    void QueueStartingWaterskin() { g_pendingStartingBottleGrant = true; }

    void CancelPendingStartingWaterskin() { g_pendingStartingBottleGrant = false; }

    void OnInGameSessionReady() {
        EnsureWaterskinFormsReady();

        if (!g_pendingStartingBottleGrant) {
            return;
        }

        RestoreStoredOrGiveDefaultBottle();

        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyEnableMod")) {
            global->value = WaterNeedManager::GetSingleton()->IsSystemEnabled() ? 1.0f : 0.0f;
        }
    }

    void OnObjectEquipped(RE::FormID baseObjectFormID, RE::TESObjectREFR* actorRef) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || actorRef != player) {
            return;
        }

        if (!HUDManager::GetSingleton()->IsInGameSession()) {
            return;
        }

        if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
            return;
        }

        if (EnsureWaterskinFormsReady(false)) {
            const auto trackedBottleIndex = FindTrackedBottleIndexByFormID(baseObjectFormID);

            if (trackedBottleIndex.has_value()) {
                if (*trackedBottleIndex >= g_trackedBottleForms.size() - 1) {
                    AddTrackedBottle(player, g_trackedBottleForms.back(), 1);
                    QueueInventoryMenuRefresh();
                    HUDManager::GetSingleton()->ShowNotification("My waterskin is empty.");
                    HUDManager::GetSingleton()->PushUpdate();
                    return;
                }

                if (IsAlreadyQuenched()) {
                    AddTrackedBottle(player, g_trackedBottleForms[*trackedBottleIndex], 1);
                    QueueInventoryMenuRefresh();
                    HUDManager::GetSingleton()->ShowNotification("I'm already Quenched.");
                    HUDManager::GetSingleton()->PushUpdate();
                    return;
                }

                if (!AddTrackedBottle(player, g_trackedBottleForms[*trackedBottleIndex + 1], 1)) {
                    HUDManager::GetSingleton()->ShowNotification("I could not drink from my waterskin.");
                    return;
                }

                WaterNeedManager::GetSingleton()->Drink(Settings::g_drinkAmount);
                QueueInventoryMenuRefresh();

                const auto currentStateText = GetCurrentStateNotificationText();
                HUDManager::GetSingleton()->ShowNotification(currentStateText.c_str());
                HUDManager::GetSingleton()->PushUpdate();
                return;
            }
        }

        TryDrinkFromHydratingDrink(baseObjectFormID);
    }

    void SetSystemEnabled(bool enabled) {
        auto* manager = WaterNeedManager::GetSingleton();
        EnsureWaterskinFormsReady(false);
        if (manager->IsSystemEnabled() == enabled) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();

        if (enabled) {
            manager->SetSystemEnabled(true);
            RestoreStoredOrGiveDefaultBottle();
        } else {
            g_pendingStartingBottleGrant = false;
            std::array<std::int32_t, 4> removedBottleCounts{};

            if (player) {
                for (std::size_t i = 0; i < g_trackedBottleForms.size(); ++i) {
                    if (!g_trackedBottleForms[i]) {
                        continue;
                    }

                    const auto count = player->GetItemCount(g_trackedBottleForms[i]);
                    if (count > 0) {
                        removedBottleCounts[i] = count;
                        player->RemoveItem(g_trackedBottleForms[i], count, RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                                           nullptr);
                    }
                }
            }

            manager->SetStoredBottleCounts(removedBottleCounts);
            manager->SetSystemEnabled(false);
        }

        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyEnableMod")) {
            global->value = enabled ? 1.0f : 0.0f;
        } else {
            logger::warn("[WaterskinUtils] IvyEnableMod global not found — is Tears of Kyne.esp loaded?");
        }

        HUDManager::GetSingleton()->RefreshSettingsMenu();
        HUDManager::GetSingleton()->PushUpdate();
    }
}