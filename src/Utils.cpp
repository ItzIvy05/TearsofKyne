#include "Utils.h"

#include <optional>

#include "Localization.h"
#include "Manager.h"
#include "Menu.h"
#include "SurvivalWidgets.h"

namespace WaterskinUtils {
    namespace {
        constexpr std::array<RE::FormID, 4> TRACKED_BOTTLE_FORM_IDS = {0x00000800, 0x00000802, 0x00000803, 0x00000804};
        constexpr std::array<const char*, 4> TRACKED_BOTTLE_EDITOR_IDS = {"IvyWaterBottle01", "IvyWaterBottle02", "IvyWaterBottle03", "IvyWaterBottle"};
        constexpr const char* REFILL_ACTIVATOR_KEYWORD_EDITOR_ID = "IvyRefillActivator";
        constexpr const char* HYDRATING_DRINK_KEYWORD_EDITOR_ID = "IvyHydratingDrinkKeyword";
        constexpr float HYDRATING_DRINK_AMOUNT = 15.0f;

        std::array<RE::TESBoundObject*, 4> g_trackedBottleForms{};
        bool g_pendingStartingBottleGrant = false;
        bool g_loggedLookupFailure = false;

        std::atomic<bool> g_fillAnimationActive{false};

        constexpr RE::FormID IDLE_PICKUP_GROUND_FORM_ID = 0x00075C3E;  // IdlePickUp_Ground
        constexpr RE::FormID IDLE_STOP_LOOSE_FORM_ID = 0x0010D9EE;     // IdleStop_Loose
        constexpr RE::FormID FILL_POWER_FORM_ID = 0x027;               // IvyFillWaterPower
        constexpr RE::FormID FOUL_LOCATIONS_FORM_ID = 0x032;           // IvyFoulWaterLocations
        constexpr RE::FormID IMMUNE_RACES_FORM_ID = 0x02F;             // IvyDirtyWaterImmuneRaces
        constexpr RE::FormID DIRTY_DISEASE_FORM_ID = 0x811;            // Ivy_DiseaseBogFever

        constexpr const char* DIRTY_WATER_ENABLED_GLOBAL_EDITOR_ID = "IvyDirtyWaterEnabled";
        constexpr std::array<RE::FormID, 3> DIRTY_BOTTLE_FORM_IDS = {0x02A, 0x02C, 0x02E};  // IvyWaterskinDirty01-03
        constexpr std::array<RE::FormID, 3> FOUL_BOTTLE_FORM_IDS = {0x029, 0x02B, 0x02D};   // IvyWaterskinFoul01-03
        std::array<RE::TESBoundObject*, 3> g_dirtyBottleForms{};
        std::array<RE::TESBoundObject*, 3> g_foulBottleForms{};

        constexpr std::array<const char*, 3> ALCOHOL_KEYWORD_EDITOR_IDS = {
            "IvyMeadBottleKeyword", "IvyWineBottleKeyword", "IvySujammaBottleKeyword"
        };

        constexpr std::array<RE::FormID, 3> EMPTY_BOTTLE_FORM_IDS = {0x923, 0x925, 0x924};   // IvyEmpty Mead / Wine / Sujamma Bottle
        constexpr std::array<RE::FormID, 3> BOTTLED_WATER_FORM_IDS = {0x920, 0x921, 0x922};  // IvyBottledWater Mead / Wine / Sujamma
        constexpr std::array<RE::FormID, 3> BOTTLED_DIRTY_FORM_IDS = {0xA2B, 0xA2C, 0xA2D};  // IvyBottledWater Mead / Wine / Sujamma Dirty
        constexpr std::array<RE::FormID, 3> BOTTLED_FOUL_FORM_IDS = {0xA2E, 0xA2F, 0xA30};   // IvyBottledWater Mead / Wine / Sujamma Foul

        std::array<RE::TESBoundObject*, 3> g_emptyReusableBottles{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterForms{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterDirtyForms{};
        std::array<RE::TESBoundObject*, 3> g_bottledWaterFoulForms{};

        bool EnsureWaterskinFormsReady(bool logFailure = true);
        void FillReusableBottles(RE::PlayerCharacter* player, bool dirty, bool foul);
        RE::TESBoundObject* FindRefillableSkinForm(RE::PlayerCharacter* player);

        bool IsAlreadyQuenched() { return WaterNeedManager::GetSingleton()->GetStage() == 0; }

        template <class T>
        T* GetForm(RE::FormID formID) {
            return RE::TESDataHandler::GetSingleton()->LookupForm<T>(formID, Settings::PLUGIN_NAME);
        }

        void ShowStateNotification() {
            TearsWidget::ShowNotification(WaterNeedManager::GetSingleton()->GetStageText().c_str());
            TearsWidget::Refresh();
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

            for (std::size_t i = 0; i < g_dirtyBottleForms.size(); ++i) {
                if ((g_dirtyBottleForms[i] && player->GetItemCount(g_dirtyBottleForms[i]) > 0) || (g_foulBottleForms[i] && player->GetItemCount(g_foulBottleForms[i]) > 0)) {
                    return true;
                }
            }

            return false;
        }

        RE::TESBoundObject* FindWaterItem(RE::FormID formID) {
            for (auto* form : g_trackedBottleForms) {
                if (form && form->GetFormID() == formID) {
                    return form;
                }
            }

            for (std::size_t i = 0; i < g_dirtyBottleForms.size(); ++i) {
                for (auto* form : {g_dirtyBottleForms[i], g_foulBottleForms[i], g_emptyReusableBottles[i], g_bottledWaterForms[i], g_bottledWaterDirtyForms[i], g_bottledWaterFoulForms[i]}) {
                    if (form && form->GetFormID() == formID) {
                        return form;
                    }
                }
            }

            return nullptr;
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

        bool IsRefillActivatorBase(RE::TESBoundObject* base) {
            return base && base->As<RE::TESObjectACTI>() && base->HasKeywordByEditorID(REFILL_ACTIVATOR_KEYWORD_EDITOR_ID);
        }

        bool IsHydratingDrink(RE::FormID baseObjectFormID) {
            auto* drink = RE::TESForm::LookupByID<RE::AlchemyItem>(baseObjectFormID);
            return drink && drink->HasKeywordByEditorID(HYDRATING_DRINK_KEYWORD_EDITOR_ID);
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

            ShowStateNotification();
        }

        bool TryFillImpl() {
            if (!WaterNeedManager::GetSingleton()->IsSystemEnabled()) {
                return false;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !EnsureWaterskinFormsReady()) {
                return false;
            }

            // wells give clean water, so a full dirty or foul skin gets swapped too
            auto* skin = FindRefillableSkinForm(player);
            if (!skin && g_dirtyBottleForms[0] && player->GetItemCount(g_dirtyBottleForms[0]) > 0) {
                skin = g_dirtyBottleForms[0];
            }
            if (!skin && g_foulBottleForms[0] && player->GetItemCount(g_foulBottleForms[0]) > 0) {
                skin = g_foulBottleForms[0];
            }
            if (skin && g_trackedBottleForms[0]) {
                player->RemoveItem(skin, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                player->AddObjectToContainer(g_trackedBottleForms[0], nullptr, 1, nullptr);
            }

            FillReusableBottles(player, false, false);

            if (!IsAlreadyQuenched()) {
                WaterNeedManager::GetSingleton()->ForceSet(0.0f);
            }

            ShowStateNotification();
            return true;
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

            static auto* foulLocations = GetForm<RE::BGSListForm>(FOUL_LOCATIONS_FORM_ID);
            auto* location = player->GetCurrentLocation();
            if (!foulLocations || !location) {
                return false;
            }

            bool found = false;
            foulLocations->ForEachForm([&](RE::TESForm* form) {
                auto* foulLocation = form->As<RE::BGSLocation>();
                if (foulLocation && (foulLocation == location || location->IsParent(foulLocation))) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return found;
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

            ShowStateNotification();
        }

        bool CanFill(RE::PlayerCharacter* player) {
            if (!IsNearWater()) {
                TearsWidget::ShowNotification(Localization::Get("$TOK_NeedWater").c_str());
                return false;
            }

            if (player->AsActorState()->IsWeaponDrawn()) {
                TearsWidget::ShowNotification(Localization::Get("$TOK_SheatheFirst").c_str());
                return false;
            }

            if (player->IsInCombat()) {
                TearsWidget::ShowNotification(Localization::Get("$TOK_CannotFillCombat").c_str());
                return false;
            }

            if (!FindRefillableSkinForm(player) && !HasEmptyReusableBottles(player)) {
                TearsWidget::ShowNotification(Localization::Get("$TOK_WaterskinFilled").c_str());
                return false;
            }

            return true;
        }

        // waterskin fill method, used from the inventory so no idle or camera change
        void FillFromItem(RE::PlayerCharacter* player) {
            if (CanFill(player)) {
                CommitWaterskinFill();
            }
        }

        RE::PlayerCharacter* GetReadyPlayer() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded() || !player->GetParentCell()) {
                return nullptr;
            }
            return player;
        }


        bool ResolveWaterskinForms(bool logSuccess, bool logFailure) {
            bool allTrackedResolved = true;
            for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                g_trackedBottleForms[i] = GetForm<RE::TESBoundObject>(TRACKED_BOTTLE_FORM_IDS[i]);
                if (!g_trackedBottleForms[i]) {
                    allTrackedResolved = false;
                }
            }

            for (std::size_t i = 0; i < DIRTY_BOTTLE_FORM_IDS.size(); ++i) {
                g_dirtyBottleForms[i] = GetForm<RE::TESBoundObject>(DIRTY_BOTTLE_FORM_IDS[i]);
                g_foulBottleForms[i] = GetForm<RE::TESBoundObject>(FOUL_BOTTLE_FORM_IDS[i]);
            }

            for (std::size_t i = 0; i < EMPTY_BOTTLE_FORM_IDS.size(); ++i) {
                g_emptyReusableBottles[i] = GetForm<RE::TESBoundObject>(EMPTY_BOTTLE_FORM_IDS[i]);
                g_bottledWaterForms[i] = GetForm<RE::TESBoundObject>(BOTTLED_WATER_FORM_IDS[i]);
                g_bottledWaterDirtyForms[i] = GetForm<RE::TESBoundObject>(BOTTLED_DIRTY_FORM_IDS[i]);
                g_bottledWaterFoulForms[i] = GetForm<RE::TESBoundObject>(BOTTLED_FOUL_FORM_IDS[i]);
            }

            if (allTrackedResolved) {
                if (logSuccess) {
                    for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                        logger::info("[WaterskinUtils] Tracked bottle '{}' (EditorID '{}').", g_trackedBottleForms[i]->GetName(), TRACKED_BOTTLE_EDITOR_IDS[i]);
                    }
                }
                g_loggedLookupFailure = false;
                return true;
            }

            if (logFailure && !g_loggedLookupFailure) {
                for (std::size_t i = 0; i < TRACKED_BOTTLE_FORM_IDS.size(); ++i) {
                    if (!g_trackedBottleForms[i]) {
                        logger::warn("[WaterskinUtils] Tracked bottle 0x{:X} / EditorID '{}' not resolved yet.", TRACKED_BOTTLE_FORM_IDS[i], TRACKED_BOTTLE_EDITOR_IDS[i]);
                    }
                }
                g_loggedLookupFailure = true;
            }

            return false;
        }

        bool EnsureWaterskinFormsReady(bool logFailure) {
            for (auto* form : g_trackedBottleForms) {
                if (!form) {
                    return ResolveWaterskinForms(false, logFailure);
                }
            }
            return true;
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
                queue->AddMessage(RE::BSFixedString(RE::InventoryMenu::MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kInventoryUpdate, nullptr);
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
                if (!AddTrackedBottle(player, g_trackedBottleForms[0], 1)) {
                    g_pendingStartingBottleGrant = true;
                    logger::warn("[WaterskinUtils] Deferred starting bottle grant because inventory was not ready yet.");
                    return;
                }

                TearsWidget::ShowNotification(Localization::Get("$TOK_ReceivedWaterskin").c_str());
            }

            g_pendingStartingBottleGrant = false;
            manager->ClearStoredBottleCounts();
            if (auto* calendar = RE::Calendar::GetSingleton()) {
                manager->SetLastGameTime(calendar->GetCurrentGameTime());
            }
            TearsWidget::Refresh();
        }

        bool IsImmuneToDirtyWater(RE::PlayerCharacter* player) {
            static auto* immuneRaces = GetForm<RE::BGSListForm>(IMMUNE_RACES_FORM_ID);
            auto* race = player ? player->GetRace() : nullptr;
            return immuneRaces && race && immuneRaces->HasForm(race);
        }

        void RollDirtyDisease(RE::PlayerCharacter* player, bool foul) {
            if (!player || IsImmuneToDirtyWater(player)) {
                return;
            }

            const float risk = foul ? Settings::g_riskFoul : Settings::g_riskLow;
            if (risk <= 0.0f) {
                return;
            }

            static clib_util::RNG rng;
            const int roll = rng.generate<int>(1, 100);
            if (roll > static_cast<int>(risk)) {
                return;
            }

            static auto* dirtyDisease = GetForm<RE::SpellItem>(DIRTY_DISEASE_FORM_ID);
            if (dirtyDisease && !player->HasSpell(dirtyDisease)) {
                player->AddSpell(dirtyDisease);
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

            ShowStateNotification();
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
                auto* sameBottle = tier == 0 ? g_bottledWaterForms[type] : tier == 1 ? g_bottledWaterDirtyForms[type] : g_bottledWaterFoulForms[type];
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

            ShowStateNotification();
            return true;
        }

        bool HandleEmptyBottleClick(RE::PlayerCharacter* player, RE::FormID baseObjectFormID) {
            for (int i = 0; i < 3; ++i) {
                if (g_emptyReusableBottles[i] && g_emptyReusableBottles[i]->GetFormID() == baseObjectFormID) {
                    AddTrackedBottle(player, g_emptyReusableBottles[i], 1);
                    QueueInventoryMenuRefresh();
                    if (Settings::g_fillMethod == Settings::FillMethod::Item) {
                        FillFromItem(player);
                        return true;
                    }
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

            auto* drink = RE::TESForm::LookupByID<RE::AlchemyItem>(baseObjectFormID);
            if (!drink) {
                return;
            }

            for (int i = 0; i < 3; ++i) {
                if (drink->HasKeywordByEditorID(ALCOHOL_KEYWORD_EDITOR_IDS[i])) {
                    if (AddTrackedBottle(player, g_emptyReusableBottles[i], 1)) {
                        QueueInventoryMenuRefresh();
                    }
                    return;
                }
            }
        }
    }


    void Initialize() {
        g_trackedBottleForms.fill(nullptr);
        g_dirtyBottleForms.fill(nullptr);
        g_foulBottleForms.fill(nullptr);
        g_emptyReusableBottles.fill(nullptr);
        g_bottledWaterForms.fill(nullptr);
        g_bottledWaterDirtyForms.fill(nullptr);
        g_bottledWaterFoulForms.fill(nullptr);
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

        if (!CanFill(player)) {
            return;
        }

        if (g_fillAnimationActive.exchange(true)) {
            return;
        }

        static auto* idlePickupGround = RE::TESForm::LookupByID<RE::TESIdleForm>(IDLE_PICKUP_GROUND_FORM_ID);
        static auto* idleStopLoose = RE::TESForm::LookupByID<RE::TESIdleForm>(IDLE_STOP_LOOSE_FORM_ID);

        auto* camera = RE::PlayerCamera::GetSingleton();
        const bool wasFirstPerson = camera && camera->IsInFirstPerson();
        if (wasFirstPerson) {
            camera->ForceThirdPerson();
        }
        PlayIdleOnPlayer(idlePickupGround);

        std::thread([wasFirstPerson] {
            auto* task = SKSE::GetTaskInterface();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (task) {
                task->AddTask([] { CommitWaterskinFill(); });
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (task) {
                task->AddTask([] { PlayIdleOnPlayer(idleStopLoose); });
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
        cell->ForEachReferenceInRange(player->GetPosition(), Settings::WATER_SCAN_RADIUS, [&](RE::TESObjectREFR* reference) {
            auto* base = reference ? reference->GetBaseObject() : nullptr;
            if (!base) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            if (base->GetFormType() == RE::FormType::Water) {
                found = true;
                return RE::BSContainer::ForEachResult::kStop;
            }

            const auto* name = base->GetName();
            if (IsRefillActivatorBase(base) || !name || !*name) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const std::string_view text(name);
            if (text.find("Water") != std::string_view::npos || text.find("River") != std::string_view::npos || text.find("Stream") != std::string_view::npos || text.find("Well") != std::string_view::npos || text.find("Lake") != std::string_view::npos || text.find("Pond") != std::string_view::npos) {
                found = true;
                return RE::BSContainer::ForEachResult::kStop;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });

        return found;
    }

    void QueueStartingWaterskin() { g_pendingStartingBottleGrant = true; }

    void CancelPendingStartingWaterskin() { g_pendingStartingBottleGrant = false; }

    void SyncFillPower() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        static auto* power = GetForm<RE::SpellItem>(FILL_POWER_FORM_ID);
        if (!power) {
            return;
        }

        const bool shouldHave = Settings::g_fillMethod == Settings::FillMethod::Power && WaterNeedManager::GetSingleton()->IsSystemEnabled();
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

    void SyncEnableGlobals(bool enabled) {
        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyEnableMod")) {
            global->value = enabled ? 1.0f : 0.0f;
        }
        if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyLeveledListEnable")) {
            global->value = enabled ? 0.0f : 100.0f;
        }
    }

    void OnInGameSessionReady() {
        EnsureWaterskinFormsReady();
        SyncFillPower();
        SyncDirtyWater();

        const bool systemEnabled = WaterNeedManager::GetSingleton()->IsSystemEnabled();

        SyncEnableGlobals(systemEnabled);

        if (!g_pendingStartingBottleGrant) {
            return;
        }

        RestoreStoredOrGiveDefaultBottle();
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
            if (auto* item = FindWaterItem(baseObjectFormID)) {
                AddTrackedBottle(player, item, 1);
                QueueInventoryMenuRefresh();
            }
            return;
        }

        if (EnsureWaterskinFormsReady(false)) {
            const auto trackedBottleIndex = FindTrackedBottleIndexByFormID(baseObjectFormID);

            if (trackedBottleIndex.has_value()) {
                if (*trackedBottleIndex >= g_trackedBottleForms.size() - 1) {
                    AddTrackedBottle(player, g_trackedBottleForms.back(), 1);
                    QueueInventoryMenuRefresh();
                    if (Settings::g_fillMethod == Settings::FillMethod::Item) {
                        FillFromItem(player);
                        return;
                    }
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

                ShowStateNotification();
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
                        player->RemoveItem(g_trackedBottleForms[i], count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    }
                }
            }

            manager->SetStoredBottleCounts(removedBottleCounts);
            manager->SetSystemEnabled(false);
        }

        SyncEnableGlobals(enabled);

        SyncFillPower();
        SyncDirtyWater();
        TearsWidget::Refresh();
        SurvivalWidgets::Refresh();
    }
}