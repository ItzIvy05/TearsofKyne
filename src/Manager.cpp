#include "Manager.h"

#include "Localization.h"
#include "Menu.h"
#include "Utils.h"

namespace {
    constexpr std::array<RE::FormID, 5> STAGE_SPELL_FORM_IDS = {0x90C, 0x018, 0x019, 0x801, 0x805};  // IvyThirst Quenched / Sated / Thirsty / Parched / Dehydrated Spell

    bool IsPlayerInJail() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        auto* loc = player->GetCurrentLocation();
        if (!loc) return false;

        if (const char* edid = loc->GetFormEditorID()) {
            const std::string_view name = edid;
            if (name == "WinterholdJail" || name == "TheChill" || name.find("Chill") != std::string_view::npos) {
                return false;
            }
        }

        for (auto* cur = loc; cur; cur = cur->parentLoc) {
            if (cur->HasKeywordString("LocTypeJail") || cur->HasKeywordString("LocTypePrison")) return true;
        }
        return false;
    }

    bool IsPlayerVampire() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        static RE::BGSKeyword* vampireKeyword = nullptr;
        if (!vampireKeyword) {
            vampireKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("Vampire");
        }
        if (vampireKeyword && player->HasKeyword(vampireKeyword)) {
            return true;
        }

        if (auto* race = player->GetRace()) {
            if (const char* edid = race->GetFormEditorID()) {
                if (std::string_view(edid).find("Vampire") != std::string_view::npos) {
                    return true;
                }
            }
        }
        return false;
    }

    float GetKynesEmbraceGlobal() {
        static RE::TESGlobal* kynesEmbraceGlobal = nullptr;
        if (!kynesEmbraceGlobal) {
            kynesEmbraceGlobal = RE::TESForm::LookupByEditorID<RE::TESGlobal>("IvyKynesEmbraceGlobal");
        }
        return kynesEmbraceGlobal ? kynesEmbraceGlobal->value : 0.0f;
    }

    std::array<RE::SpellItem*, 5> g_stageSpells{};

    RE::SpellItem* ResolveStageSpell(int stage) {
        auto& spell = g_stageSpells[stage];
        if (!spell) {
            spell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(STAGE_SPELL_FORM_IDS[stage], Settings::PLUGIN_NAME);
        }
        return spell;
    }

    void SyncHydrationStageSpell(int currentStage, bool systemEnabled) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        for (int i = 0; i < static_cast<int>(STAGE_SPELL_FORM_IDS.size()); ++i) {
            auto* spell = ResolveStageSpell(i);
            if (!spell) {
                continue;
            }

            const bool shouldHaveSpell = systemEnabled && i == currentStage;

            if (shouldHaveSpell) {
                if (!player->HasSpell(spell)) {
                    player->AddSpell(spell);
                    logger::info("[Tears of Kyne] Applied hydration spell '{}'.", spell->GetName());
                }
            } else {
                if (player->HasSpell(spell)) {
                    player->RemoveSpell(spell);
                    logger::info("[Tears of Kyne] Removed hydration spell '{}'.", spell->GetName());
                }
            }
        }
    }

}

std::string WaterNeedManager::GetStageText() const {
    static constexpr std::array<const char*, 5> keys = {"$TOK_StageQuenched", "$TOK_StageSated", "$TOK_StageThirsty", "$TOK_StageParched", "$TOK_StageDehydrated"};
    return Localization::Get(keys[GetStage()]);
}

void WaterNeedManager::InitializeFromSettings() {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);

        if (_lastGameTime < 0.0f) {
            _lastGameTime = GetCurrentGameTime();
        }

        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
}

void WaterNeedManager::OnNewGame() {
    {
        std::scoped_lock lock(_mutex);
        _thirstLevel = 0.0f;
        _lastGameTime = GetCurrentGameTime();
        _hoursWithoutDrink = 0.0f;
        _systemEnabled = true;
        _storedBottleCounts.fill(0);
    }

    SyncHydrationStageSpell(0, true);
    TearsWidget::Refresh();
}

void WaterNeedManager::OnGameLoaded() {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);
        _lastGameTime = GetCurrentGameTime();
        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
    TearsWidget::Refresh();
}

void WaterNeedManager::Tick() {
    if (!TearsWidget::IsInGameSession()) {
        return;
    }

    WaterskinUtils::OnInGameSessionReady();

    if (!IsSystemEnabled()) {
        SyncHydrationStageSpell(-1, false);
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->Is3DLoaded()) {
        return;
    }

    const auto now = GetCurrentGameTime();
    if (now < 0.0f) {
        return;
    }

    int previousStage = 0;
    int currentStage = 0;
    bool changed = false;
    bool systemEnabled = true;
    float hoursWithoutDrink = 0.0f;
    const bool vampirePaused = Settings::g_disableForVampire && IsPlayerVampire();

    {
        std::scoped_lock lock(_mutex);

        if (_lastGameTime < 0.0f) {
            _lastGameTime = now;
            return;
        }

        const auto hoursPassed = (now - _lastGameTime) * 24.0f;
        _lastGameTime = now;

        previousStage = StageFromLevel(_thirstLevel);

        if (hoursPassed > 0.0f) {
            const bool jailPaused = Settings::g_pauseNeedsInJail && IsPlayerInJail();

            if (!jailPaused && !vampirePaused) {
                float rate = Settings::g_thirstRate;
                if (Settings::g_enablePerkGate && Settings::PlayerHasGatePerk()) {
                    const float reduction = Settings::g_useKynesEmbraceGlobal ? std::clamp(GetKynesEmbraceGlobal(), 0.0f, 100.0f) : std::clamp(Settings::g_perkRateReduction, 15.0f, 75.0f);
                    rate *= (1.0f - reduction / 100.0f);
                }
                _thirstLevel = std::clamp(_thirstLevel + (hoursPassed * rate), 0.0f, 100.0f);
                _hoursWithoutDrink += hoursPassed;
                changed = true;
            }
        }

        currentStage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
        hoursWithoutDrink = _hoursWithoutDrink;
    }

    SyncHydrationStageSpell(vampirePaused ? -1 : currentStage, systemEnabled);
    TearsWidget::Refresh();

    if (changed && currentStage > previousStage) {
        TearsWidget::ShowNotification(GetStageText().c_str());
    }

    if (systemEnabled && !vampirePaused && Settings::g_deathByDehydration && hoursWithoutDrink >= Settings::DEATH_HOURS_WITHOUT_WATER) {
        
        if (!player->IsDead()) {
            TearsWidget::ShowNotification(Localization::Get("$TOK_DeathThirst").c_str());
            player->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -1000000.0f);
            logger::info("[Tears of Kyne] Player died of dehydration.");
        }
    }
}

void WaterNeedManager::Drink(float amount) {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);
        _thirstLevel = std::clamp(_thirstLevel - amount, 0.0f, 100.0f);
        _hoursWithoutDrink = 0.0f;
        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
    TearsWidget::Refresh();
}

void WaterNeedManager::ForceSet(float value) {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);
        _thirstLevel = std::clamp(value, 0.0f, 100.0f);
        if (_thirstLevel <= 0.0f) {
            _hoursWithoutDrink = 0.0f;
        }
        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
}

bool WaterNeedManager::IsSystemEnabled() const {
    std::scoped_lock lock(_mutex);
    return _systemEnabled;
}

bool WaterNeedManager::IsPausedForVampire() const { return Settings::g_disableForVampire && IsPlayerVampire(); }

void WaterNeedManager::SetSystemEnabled(bool enabled) {
    int stage = 0;

    {
        std::scoped_lock lock(_mutex);
        _systemEnabled = enabled;
        stage = StageFromLevel(_thirstLevel);
    }

    SyncHydrationStageSpell(stage, enabled);
}

std::array<std::int32_t, 4> WaterNeedManager::GetStoredBottleCounts() const {
    std::scoped_lock lock(_mutex);
    return _storedBottleCounts;
}

void WaterNeedManager::SetStoredBottleCounts(const std::array<std::int32_t, 4>& counts) {
    std::scoped_lock lock(_mutex);
    _storedBottleCounts = counts;
}

void WaterNeedManager::ClearStoredBottleCounts() {
    std::scoped_lock lock(_mutex);
    _storedBottleCounts.fill(0);
}

float WaterNeedManager::GetLevel() const {
    std::scoped_lock lock(_mutex);
    return _thirstLevel;
}


int WaterNeedManager::GetStage() const { return StageFromLevel(GetLevel()); }


float WaterNeedManager::GetLastGameTime() const {
    std::scoped_lock lock(_mutex);
    return _lastGameTime;
}

void WaterNeedManager::SetLastGameTime(float value) {
    std::scoped_lock lock(_mutex);
    _lastGameTime = value;
}

float WaterNeedManager::GetHoursWithoutDrink() const {
    std::scoped_lock lock(_mutex);
    return _hoursWithoutDrink;
}

void WaterNeedManager::SetHoursWithoutDrink(float value) {
    std::scoped_lock lock(_mutex);
    _hoursWithoutDrink = std::max(0.0f, value);
}

int WaterNeedManager::StageFromLevel(float level) const {
    if (level < STAGE_BOUNDS[1]) return 0;
    if (level < STAGE_BOUNDS[2]) return 1;
    if (level < STAGE_BOUNDS[3]) return 2;
    if (level < STAGE_BOUNDS[4]) return 3;
    return 4;
}

float WaterNeedManager::GetCurrentGameTime() const {
    auto* calendar = RE::Calendar::GetSingleton();
    return calendar ? calendar->GetCurrentGameTime() : -1.0f;
}