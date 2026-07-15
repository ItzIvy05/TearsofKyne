#include "Utils.h"

#include <optional>
#include <random>

#include "Localization.h"
#include "Manager.h"
#include "Menu.h"

namespace WaterskinUtils {
    namespace {
        constexpr std::array<RE::FormID, 4> TRACKED_BOTTLE_FORM_IDS = {0x00000800, 0x00000802, 0x00000803, 0x00000804};
        constexpr std::array<const char*, 4> TRACKED_BOTTLE_EDITOR_IDS = {"IvyWaterBottle01", "IvyWaterBottle02", "IvyWaterBottle03", "IvyWaterBottle"};
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

        constexpr RE::FormID IDLE_PICKUP_GROUND_FORM_ID = 0x00075C3E;
        constexpr RE::FormID IDLE_STOP_LOOSE_FORM_ID = 0x0010D9EE;
        RE::TESIdleForm* g_idlePickupGround = nullptr;
        RE::TESIdleForm* g_idleStopLoose = nullptr;
        std::atomic<bool> g_fillAnimationActive{false};

        constexpr const char* FILL_POWER_EDITOR_ID = "IvyFillWaterPower";
        RE::SpellItem* g_fillPower = nullptr;
        bool g_loggedFillPowerFailure = false;

        constexpr const char* DIRTY_WATER_ENABLED_GLOBAL_EDITOR_ID = "IvyDirtyWaterEnabled";
        constexpr const char* FOUL_LOCATIONS_LIST_EDITOR_ID = "IvyFoulWaterLocations";
        constexpr const char* DIRTY_DISEASE_EDITOR_ID = "Ivy_DiseaseBogFever";
        constexpr const char* IMMUNE_RACES_LIST_EDITOR_ID = "IvyDirtyWaterImmuneRaces";
        constexpr std::array<const char*, 3> DIRTY_BOTTLE_EDITOR_IDS = {"IvyWaterskinDirty01", "IvyWaterskinDirty02", "IvyWaterskinDirty03"};
        constexpr std::array<const char*, 3> FOUL_BOTTLE_EDITOR_IDS = {"IvyWaterskinFoul01", "IvyWaterskinFoul02", "IvyWaterskinFoul03"};
        std::array<RE::TESBoundObject*, 3> g_dirtyBottleForms{};
        std::array<RE::TESBoundObject*, 3> g_foulBottleForms{};
        RE::SpellItem* g_dirtyDisease = nullptr;
        RE::BGSListForm* g_immuneRaces = nullptr;
        RE::BGSListForm* g_foulLocations = nullptr;

        constexpr std::array<const char*, 3> ALCOHOL_KEYWORD_EDITOR_IDS = {
            "IvyMeadBottleKeyword", "IvyWineBottleKeyword", "IvySujammaBottleKeyword"
        };

        constexpr std::array<const char*, 3> EMPTY_BOTTLE_EDITOR_IDS = {
            "IvyEmptyMeadBottle", "IvyEmptyWineBottle","IvyEmptySujammaBottle"
        };

        constexpr std::array<const char*, 3> BOTTLED_WATER_EDITOR_IDS = {
            "IvyBottledWaterMead", "IvyBottledWaterWine","IvyBottledWaterSujamma"
        };

        constexpr std::array<const char*, 3> BOTTLED_DIRTY_EDITOR_IDS = {
            "IvyBottledWaterMeadDirty", "IvyBottledWaterWineDirty", "IvyBottledWaterSujammaDirty"};
        constexpr std::array<const char*, 3> BOTTLED_FOUL_EDITOR_IDS = {
            "IvyBottledWaterMeadFoul", "IvyBottledWaterWineFoul", "IvyBottledWaterSujammaFoul"};

        std::array<RE::TESBoundObject*, 3> g_emptyReusableBottles{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterForms{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterDirtyForms{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterFoulForms{};

        float GetCurrentGameTime() {
            auto* calendar = RE::Calendar::GetSingleton();
            return calendar ? calendar->GetCurrentGameTime() : -1.0f;
        }

        bool EnsureWaterskinFormsReady(bool logFailure = true);
        void FillReusableBottles(RE::PlayerCharacter* player, bool dirty, bool foul);

        bool IsAlreadyQuenched() { return WaterNeedManager::GetSingleton()->GetStage() == 0; }

        std::string GetCurrentStateNotificationText() {
            const auto* manager = WaterNeedManager::GetSingleton();
            const int stage = manager ? manager->GetStage() : 0;
            const char* key = "$TOK_StageQuenched";
            switch (stage) {
                case 0:
                    key = "$TOK_StageQuenched";
                    break;
                case 1:
                    key = "$TOK_StageSated";
                    break;
                case 2:
                    key = "$TOK_StageThirsty";
                    break;
                case 3:
                    key = "$TOK_StageParched";
                    break;
                case 4:
                    key = "$TOK_StageDehydrated";
                    break;
                default:
                    key = "$TOK_StageQuenched";
                    break;
            }
            return Localization::Get(key);
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
                TearsWidget::ShowNotification(Localization::Get("$TOK_AlreadyQuenched").c_str());
                TearsWidget::Refresh();
                return;
            }

            WaterNeedManager::GetSingleton()->Drink(HYDRATING_DRINK_AMOUNT);

            const auto currentStateText = GetCurrentStateNotificationText();
            TearsWidget::ShowNotification(currentStateText.c_str());
            TearsWidget::Refresh();
        }

        bool TryFillImpl() {
            if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
                return false;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !EnsureWaterskinFormsReady()) {
                return false;
            }

            const auto refillableBottleIndex = FindRefillableBottleIndex(player);
            if (refillableBottleIndex.has_value()) {
                ReplaceTrackedBottle(player, *refillableBottleIndex, 0);
            }

            FillReusableBottles(player, false, false);

            if (!IsAlreadyQuenched()) {
                WaterNeedManager::GetSingleton()->ForceSet(0.0f);
            }

            const auto currentStateText = GetCurrentStateNotificationText();
            TearsWidget::ShowNotification(currentStateText.c_str());
            TearsWidget::Refresh();
            return true;
        }

        RE::TESIdleForm* LookupIdle(RE::FormID formID, const char* editorID) {
            if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
                if (auto* form = dataHandler->LookupForm<RE::TESIdleForm>(formID, "Skyrim.esm")) {
                    return form;
                }
            }
            return RE::TESForm::LookupByEditorID<RE::TESIdleForm>(editorID);
        }

        void EnsureFillAnimationFormsReady() {
            if (!g_idlePickupGround) {
                g_idlePickupGround = LookupIdle(IDLE_PICKUP_GROUND_FORM_ID, "IdlePickUp_Ground");
            }
            if (!g_idleStopLoose) {
                g_idleStopLoose = LookupIdle(IDLE_STOP_LOOSE_FORM_ID, "IdleStop_Loose");
            }
        }

        RE::SpellItem* ResolveFillPower() {
            if (g_fillPower) {
                return g_fillPower;
            }
            g_fillPower = RE::TESForm::LookupByEditorID<RE::SpellItem>(FILL_POWER_EDITOR_ID);
            if (!g_fillPower && !g_loggedFillPowerFailure) {
                logger::warn("[WaterskinUtils] Could not resolve fill power '{}'.", FILL_POWER_EDITOR_ID);
                g_loggedFillPowerFailure = true;
            }
            return g_fillPower;
        }

        void PlayIdleOnPlayer(RE::TESIdleForm* idle) {
            if (!idle) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }
            auto* process = player->GetActorRuntimeData().currentProcess;
            if (process) {
                process->PlayIdle(player, idle, player);
            }
        }

        bool IsInFoulRegion() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return false;
            }

            if (!g_foulLocations) {
                g_foulLocations = RE::TESForm::LookupByEditorID<RE::BGSListForm>(FOUL_LOCATIONS_LIST_EDITOR_ID);
            }
            if (!g_foulLocations) {
                return false;
            }

            auto* location = player->GetCurrentLocation();
            int guard = 0;
            while (location && guard++ < 16) {
                if (g_foulLocations->HasForm(location)) {
                    return true;
                }
                location = location->parentLoc;
            }
            return false;
        }

        RE::TESBoundObject* FindRefillableSkinForm(RE::PlayerCharacter* player) {
            if (!player) {
                return nullptr;
            }

            if (g_trackedBottleForms[3] && player->GetItemCount(g_trackedBottleForms[3]) > 0) {
                return g_trackedBottleForms[3];
            }

            RE::TESBoundObject* partials[] = {g_trackedBottleForms[2], g_dirtyBottleForms[2], g_foulBottleForms[2], g_trackedBottleForms[1], g_dirtyBottleForms[1], g_foulBottleForms[1]};
            for (auto* form : partials) {
                if (form && player->GetItemCount(form) > 0) {
                    return form;
                }
            }

            return nullptr;
        }

        bool HasEmptyReusableBottles(RE::PlayerCharacter* player) {
            if (!player || !Settings::g_reuseBottles) {
                return false;
            }
            for (auto* form : g_emptyReusableBottles) {
                if (form && player->GetItemCount(form) > 0) {
                    return true;
                }
            }
            return false;
        }

        void FillReusableBottles(RE::PlayerCharacter* player, bool dirty, bool foul) {
            if (!player || !Settings::g_reuseBottles) {
                return;
            }

            for (std::size_t i = 0; i < g_emptyReusableBottles.size(); ++i) {
                auto* empty = g_emptyReusableBottles[i];
                if (!empty) {
                    continue;
                }

                RE::TESBoundObject* target = g_bottledWaterForms[i];
                if (dirty) {
                    auto* tierTarget = foul ? g_bottledWaterFoulForms[i] : g_bottledWaterDirtyForms[i];
                    if (tierTarget) {
                        target = tierTarget;
                    }
                }
                if (!target) {
                    continue;
                }

                const auto count = player->GetItemCount(empty);
                if (count <= 0) {
                    continue;
                }

                player->RemoveItem(empty, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                player->AddObjectToContainer(target, nullptr, count, nullptr);
            }
        }

        void CommitWaterskinFill() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            const bool dirty = Settings::g_enableDirtyWater && WaterNeedManager::GetSingleton()->IsSystemEnabled();
            const bool foul = dirty && IsInFoulRegion();

            auto* refillable = FindRefillableSkinForm(player);
            if (refillable) {
                RE::TESBoundObject* targetFull = g_trackedBottleForms[0];
                if (dirty) {
                    auto* dirtyTarget = foul ? g_foulBottleForms[0] : g_dirtyBottleForms[0];
                    if (dirtyTarget) {
                        targetFull = dirtyTarget;
                    }
                }

                if (targetFull) {
                    player->RemoveItem(refillable, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    player->AddObjectToContainer(targetFull, nullptr, 1, nullptr);
                }
            }

            FillReusableBottles(player, dirty, foul);

            const auto currentStateText = GetCurrentStateNotificationText();
            TearsWidget::ShowNotification(currentStateText.c_str());
            TearsWidget::Refresh();
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

        RE::TESBoundObject* LookupBoundObjectWithFallback(RE::TESDataHandler* dataHandler, RE::FormID formID, const char* editorID) {
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

            g_filledWaterskin = LookupBoundObjectWithFallback(dataHandler, Settings::g_filledWaterskinFormID, FILLED_WATERSKIN_EDITOR_ID);
            g_emptyWaterskin = LookupBoundObjectWithFallback(dataHandler, Settings::g_emptyWaterskinFormID, EMPTY_WATERSKIN_EDITOR_ID);

            bool allTrackedResolved = true;
            for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                g_trackedBottleForms[i] = LookupBoundObjectWithFallback(dataHandler, TRACKED_BOTTLE_FORM_IDS[i],
                                                                        TRACKED_BOTTLE_EDITOR_IDS[i]);
                if (!g_trackedBottleForms[i]) {
                    allTrackedResolved = false;
                }
            }

            for (std::size_t i = 0; i < DIRTY_BOTTLE_EDITOR_IDS.size(); ++i) {
                g_dirtyBottleForms[i] = LookupBoundObjectByEditorID(DIRTY_BOTTLE_EDITOR_IDS[i]);
                g_foulBottleForms[i] = LookupBoundObjectByEditorID(FOUL_BOTTLE_EDITOR_IDS[i]);
            }

            for (std::size_t i = 0; i < EMPTY_BOTTLE_EDITOR_IDS.size(); ++i) {
                g_emptyReusableBottles[i] = LookupBoundObjectByEditorID(EMPTY_BOTTLE_EDITOR_IDS[i]);
                g_bottledWaterForms[i] = LookupBoundObjectByEditorID(BOTTLED_WATER_EDITOR_IDS[i]);
                g_bottledWaterDirtyForms[i] = LookupBoundObjectByEditorID(BOTTLED_DIRTY_EDITOR_IDS[i]);
                g_bottledWaterFoulForms[i] = LookupBoundObjectByEditorID(BOTTLED_FOUL_EDITOR_IDS[i]);
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

                TearsWidget::ShowNotification(Localization::Get("$TOK_ReceivedWaterskin").c_str());
            }

            g_pendingStartingBottleGrant = false;
            manager->ClearStoredBottleCounts();
            const auto now = GetCurrentGameTime();
            if (now >= 0.0f) {
                manager->SetLastGameTime(now);
            }
            TearsWidget::Refresh();
        }

        bool IsImmuneToDirtyWater(RE::PlayerCharacter* player) {
            if (!g_immuneRaces) {
                g_immuneRaces = RE::TESForm::LookupByEditorID<RE::BGSListForm>(IMMUNE_RACES_LIST_EDITOR_ID);
            }
            if (!g_immuneRaces || !player) {
                return false;
            }
            auto* race = player->GetRace();
            return race && g_immuneRaces->HasForm(race);
        }

        void RollDirtyDisease(RE::PlayerCharacter* player, bool foul) {
            if (!player || IsImmuneToDirtyWater(player)) {
                return;
            }

            const float risk = foul ? Settings::g_riskFoul : Settings::g_riskLow;
            if (risk <= 0.0f) {
                return;
            }

            static std::mt19937 rng{std::random_device{}()};
            const int roll = std::uniform_int_distribution<int>(1, 100)(rng);
            if (roll > static_cast<int>(risk)) {
                return;
            }

            if (!g_dirtyDisease) {
                g_dirtyDisease = RE::TESForm::LookupByEditorID<RE::SpellItem>(DIRTY_DISEASE_EDITOR_ID);
            }
            if (g_dirtyDisease && !player->HasSpell(g_dirtyDisease)) {
                player->AddSpell(g_dirtyDisease);
                logger::info("[WaterskinUtils] Applied dirty water sickness.");
            }
        }

        bool HandleDirtyDrink(RE::PlayerCharacter* player, RE::FormID baseObjectFormID) {
            int charge = -1;
            bool foul = false;
            for (int i = 0; i < 3; ++i) {
                if (g_dirtyBottleForms[i] && g_dirtyBottleForms[i]->GetFormID() == baseObjectFormID) {
                    charge = i;
                    foul = false;
                    break;
                }
                if (g_foulBottleForms[i] && g_foulBottleForms[i]->GetFormID() == baseObjectFormID) {
                    charge = i;
                    foul = true;
                    break;
                }
            }

            if (charge < 0) {
                return false;
            }

            auto& tierForms = foul ? g_foulBottleForms : g_dirtyBottleForms;

            if (IsAlreadyQuenched()) {
                AddTrackedBottle(player, tierForms[charge], 1);
                QueueInventoryMenuRefresh();
                TearsWidget::ShowNotification(Localization::Get("$TOK_AlreadyQuenched").c_str());
                TearsWidget::Refresh();
                return true;
            }

            RE::TESBoundObject* nextForm = (charge < 2) ? tierForms[charge + 1] : g_trackedBottleForms[3];
            if (!AddTrackedBottle(player, nextForm, 1)) {
                TearsWidget::ShowNotification(Localization::Get("$TOK_CouldNotDrink").c_str());
                return true;
            }

            WaterNeedManager::GetSingleton()->Drink(Settings::g_drinkAmount);
            RollDirtyDisease(player, foul);
            QueueInventoryMenuRefresh();

            const auto currentStateText = GetCurrentStateNotificationText();
            TearsWidget::ShowNotification(currentStateText.c_str());
            TearsWidget::Refresh();
            return true;
        }

        bool HandleBottledWaterDrink(RE::PlayerCharacter* player, RE::FormID baseObjectFormID) {
            int type = -1;
            int tier = -1;
            for (int i = 0; i < 3; ++i) {
                if (g_bottledWaterForms[i] && g_bottledWaterForms[i]->GetFormID() == baseObjectFormID) {
                    type = i;
                    tier = 0;
                    break;
                }
                if (g_bottledWaterDirtyForms[i] && g_bottledWaterDirtyForms[i]->GetFormID() == baseObjectFormID) {
                    type = i;
                    tier = 1;
                    break;
                }
                if (g_bottledWaterFoulForms[i] && g_bottledWaterFoulForms[i]->GetFormID() == baseObjectFormID) {
                    type = i;
                    tier = 2;
                    break;
                }
            }

            if (type < 0) {
                return false;
            }

            if (IsAlreadyQuenched()) {
                auto* sameBottle = (tier == 0)   ? g_bottledWaterForms[type]
                                   : (tier == 1) ? g_bottledWaterDirtyForms[type]
                                                 : g_bottledWaterFoulForms[type];
                AddTrackedBottle(player, sameBottle, 1);
                QueueInventoryMenuRefresh();
                TearsWidget::ShowNotification(Localization::Get("$TOK_AlreadyQuenched").c_str());
                TearsWidget::Refresh();
                return true;
            }

            AddTrackedBottle(player, g_emptyReusableBottles[type], 1);
            WaterNeedManager::GetSingleton()->Drink(std::clamp(Settings::g_bottleQuench, 15.0f, 75.0f));
            if (tier > 0) {
                RollDirtyDisease(player, tier == 2);
            }
            QueueInventoryMenuRefresh();

            const auto currentStateText = GetCurrentStateNotificationText();
            TearsWidget::ShowNotification(currentStateText.c_str());
            TearsWidget::Refresh();
            return true;
        }

        bool HandleEmptyBottleClick(RE::PlayerCharacter* player, RE::FormID baseObjectFormID) {
            for (int i = 0; i < 3; ++i) {
                if (g_emptyReusableBottles[i] && g_emptyReusableBottles[i]->GetFormID() == baseObjectFormID) {
                    AddTrackedBottle(player, g_emptyReusableBottles[i], 1);
                    QueueInventoryMenuRefresh();
                    TearsWidget::ShowNotification(Localization::Get("$TOK_BottleEmpty").c_str());
                    return true;
                }
            }
            return false;
        }

        void HandleAlcoholBottleReturn(RE::PlayerCharacter* player, RE::FormID baseObjectFormID) {
            if (!Settings::g_reuseBottles) {
                return;
            }

            auto* form = RE::TESForm::LookupByID(baseObjectFormID);
            if (!form) {
                return;
            }

            auto* boundObject = form->As<RE::TESBoundObject>();
            if (!boundObject || !boundObject->As<RE::AlchemyItem>()) {
                return;
            }

            for (int i = 0; i < 3; ++i) {
                if (boundObject->HasKeywordByEditorID(ALCOHOL_KEYWORD_EDITOR_IDS[i])) {
                    if (AddTrackedBottle(player, g_emptyReusableBottles[i], 1)) {
                        QueueInventoryMenuRefresh();
                    }
                    return;
                }
            }
        }
    }

    void Initialize() {
        g_filledWaterskin = nullptr;
        g_emptyWaterskin = nullptr;
        g_trackedBottleForms.fill(nullptr);
        g_dirtyBottleForms.fill(nullptr);
        g_foulBottleForms.fill(nullptr);
        g_emptyReusableBottles.fill(nullptr);
        g_bottledWaterForms.fill(nullptr);
        g_bottledWaterDirtyForms.fill(nullptr);
        g_bottledWaterFoulForms.fill(nullptr);
        g_dirtyDisease = nullptr;
        g_immuneRaces = nullptr;
        g_foulLocations = nullptr;
        g_pendingStartingBottleGrant = false;
        g_loggedLookupFailure = false;
        ResolveWaterskinForms(true, false);
    }

    void TryFill() {
        if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !EnsureWaterskinFormsReady()) {
            return;
        }

        if (!IsNearWater()) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_NeedWater").c_str());
            return;
        }

        if (player->AsActorState()->IsWeaponDrawn()) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_SheatheFirst").c_str());
            return;
        }

        if (player->IsInCombat()) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_CannotFillCombat").c_str());
            return;
        }

        if (!FindRefillableSkinForm(player) && !HasEmptyReusableBottles(player)) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_WaterskinFilled").c_str());
            return;
        }

        if (g_fillAnimationActive.exchange(true)) {
            return;
        }

        EnsureFillAnimationFormsReady();

        auto* camera = RE::PlayerCamera::GetSingleton();
        const bool wasFirstPerson = camera && camera->IsInFirstPerson();
        if (wasFirstPerson) {
            camera->ForceThirdPerson();
        }
        PlayIdleOnPlayer(g_idlePickupGround);

        std::thread([wasFirstPerson] {
            auto* task = SKSE::GetTaskInterface();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (task) {
                task->AddTask([] { CommitWaterskinFill(); });
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (task) {
                task->AddTask([] { PlayIdleOnPlayer(g_idleStopLoose); });
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (task) {
                task->AddTask([wasFirstPerson] {
                    if (wasFirstPerson) {
                        if (auto* restoreCamera = RE::PlayerCamera::GetSingleton()) {
                            restoreCamera->ForceFirstPerson();
                        }
                    }
                });
            }
            g_fillAnimationActive.store(false);
        }).detach();
    }

    bool TryFillFromActivator(RE::TESObjectREFR* activatedRef, RE::TESObjectREFR* actionRef) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || actionRef != player || !activatedRef) {
            return false;
        }

        auto* base = activatedRef->GetBaseObject();
        if (!IsRefillActivatorBase(base)) {
            return false;
        }

        return TryFillImpl();
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
                TearsWidget::ShowNotification(Localization::Get("$TOK_WaterskinEmpty").c_str());
            } else {
                TearsWidget::ShowNotification(Localization::Get("$TOK_NoFilledWaterskin").c_str());
            }
            return;
        }

        if (IsAlreadyQuenched()) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_AlreadyQuenched").c_str());
            return;
        }

        if (!ReplaceTrackedBottle(player, *drinkableBottleIndex, *drinkableBottleIndex + 1)) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_CouldNotDrink").c_str());
            return;
        }

        WaterNeedManager::GetSingleton()->Drink(Settings::g_drinkAmount);
        QueueInventoryMenuRefresh();
        const auto currentStateText = GetCurrentStateNotificationText();
        TearsWidget::ShowNotification(currentStateText.c_str());
        TearsWidget::Refresh();
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

    void SyncFillPower() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        auto* power = ResolveFillPower();
        if (!power) {
            return;
        }

        const bool shouldHave = Settings::g_useFillPower && WaterNeedManager::GetSingleton()->IsSystemEnabled();
        const bool hasIt = player->HasSpell(power);

        if (shouldHave && !hasIt) {
            player->AddSpell(power);
            logger::info("[WaterskinUtils] Fill power added to player.");
        } else if (!shouldHave && hasIt) {
            player->RemoveSpell(power);
            logger::info("[WaterskinUtils] Fill power removed from player.");
        }
    }

    void SyncDirtyWater() {
        const bool enabled = Settings::g_enableDirtyWater && WaterNeedManager::GetSingleton()->IsSystemEnabled();

        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(DIRTY_WATER_ENABLED_GLOBAL_EDITOR_ID)) {
            global->value = enabled ? 1.0f : 0.0f;
        }
    }

    void OnInGameSessionReady() {
        EnsureWaterskinFormsReady();
        SyncFillPower();
        SyncDirtyWater();

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

        if (!TearsWidget::IsInGameSession()) {
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
                    TearsWidget::ShowNotification(Localization::Get("$TOK_WaterskinEmpty").c_str());
                    TearsWidget::Refresh();
                    return;
                }

                if (IsAlreadyQuenched()) {
                    AddTrackedBottle(player, g_trackedBottleForms[*trackedBottleIndex], 1);
                    QueueInventoryMenuRefresh();
                    TearsWidget::ShowNotification(Localization::Get("$TOK_AlreadyQuenched").c_str());
                    TearsWidget::Refresh();
                    return;
                }

                if (!AddTrackedBottle(player, g_trackedBottleForms[*trackedBottleIndex + 1], 1)) {
                    TearsWidget::ShowNotification(Localization::Get("$TOK_CouldNotDrink").c_str());
                    return;
                }

                WaterNeedManager::GetSingleton()->Drink(Settings::g_drinkAmount);
                QueueInventoryMenuRefresh();

                const auto currentStateText = GetCurrentStateNotificationText();
                TearsWidget::ShowNotification(currentStateText.c_str());
                TearsWidget::Refresh();
                return;
            }

            if (HandleDirtyDrink(player, baseObjectFormID)) {
                return;
            }

            if (HandleBottledWaterDrink(player, baseObjectFormID)) {
                return;
            }

            if (HandleEmptyBottleClick(player, baseObjectFormID)) {
                return;
            }

            HandleAlcoholBottleReturn(player, baseObjectFormID);
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
            logger::warn("[WaterskinUtils] IvyEnableMod global not found, is Tears of Kyne.esp loaded?");
        }

        SyncFillPower();
        SyncDirtyWater();
        TearsWidget::Refresh();
    }
}