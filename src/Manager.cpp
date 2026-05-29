#include "Manager.h"

#include "MCP.h"
#include "Utils.h"

namespace {
    constexpr std::array<const char*, 5> STAGE_SPELL_EDITOR_IDS = {
        "IvyThirstQuenchedSpell", 
        "IvyThirstSatedSpell",
        "IvyThirstThirstySpell",
        "IvyThirstParchedSpell",
        "IvyThirstDehydratedSpell"
    };

    std::array<RE::SpellItem*, 5> g_stageSpells{};
    std::array<bool, 5> g_loggedStageSpellLookupFailure{};

    RE::SpellItem* ResolveStageSpell(int stage) {
        if (stage < 0 || stage >= static_cast<int>(STAGE_SPELL_EDITOR_IDS.size())) {
            return nullptr;
        }

        if (g_stageSpells[stage]) {
            return g_stageSpells[stage];
        }

        const char* editorID = STAGE_SPELL_EDITOR_IDS[stage];
        g_stageSpells[stage] = RE::TESForm::LookupByEditorID<RE::SpellItem>(editorID);

        if (!g_stageSpells[stage] && !g_loggedStageSpellLookupFailure[stage]) {
            logger::warn("[Tears of Kyne] Could not resolve hydration spell '{}'.", editorID);
            g_loggedStageSpellLookupFailure[stage] = true;
        }

        if (g_stageSpells[stage]) {
            g_loggedStageSpellLookupFailure[stage] = false;
        }

        return g_stageSpells[stage];
    }

    void SyncHydrationStageSpell(int currentStage, bool systemEnabled) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        for (int i = 0; i < static_cast<int>(STAGE_SPELL_EDITOR_IDS.size()); ++i) {
            auto* spell = ResolveStageSpell(i);
            if (!spell) {
                continue;
            }

            const bool shouldHaveSpell = systemEnabled && i == currentStage;

            if (shouldHaveSpell) {
                if (!player->HasSpell(spell)) {
                    player->AddSpell(spell);
                    logger::info("[Tears of Kyne] Applied hydration spell '{}'.", STAGE_SPELL_EDITOR_IDS[i]);
                }
            } else {
                if (player->HasSpell(spell)) {
                    player->RemoveSpell(spell);
                    logger::info("[Tears of Kyne] Removed hydration spell '{}'.", STAGE_SPELL_EDITOR_IDS[i]);
                }
            }
        }
    }

    const char* GetStageNotificationText(int stage) {
        switch (stage) {
            case 0:
                return "I'm Quenched.";
            case 1:
                return "I'm Sated.";
            case 2:
                return "I'm Thirsty.";
            case 3:
                return "I'm Parched.";
            default:
                return "I'm Dehydrated.";
        }
    }
}

WaterNeedManager* WaterNeedManager::GetSingleton() {
    static WaterNeedManager instance;
    return &instance;
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
        _systemEnabled = true;
        _storedBottleCounts.fill(0);
    }

    SyncHydrationStageSpell(0, true);
    HUDManager::GetSingleton()->PushUpdate();
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
    HUDManager::GetSingleton()->PushUpdate();
}

void WaterNeedManager::Tick() {
    if (!HUDManager::GetSingleton()->IsInGameSession()) {
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
            _thirstLevel = std::clamp(_thirstLevel + (hoursPassed * Settings::g_thirstRate), 0.0f, 100.0f);
            changed = true;
        }

        currentStage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(currentStage, systemEnabled);
    HUDManager::GetSingleton()->PushUpdate();

    if (changed && currentStage > previousStage) {
        HUDManager::GetSingleton()->ShowNotification(GetStageNotificationText(currentStage));
    }
}

void WaterNeedManager::Drink(float amount) {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);
        _thirstLevel = std::clamp(_thirstLevel - amount, 0.0f, 100.0f);
        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
    HUDManager::GetSingleton()->PushUpdate();
}

void WaterNeedManager::ForceSet(float value) {
    int stage = 0;
    bool systemEnabled = true;

    {
        std::scoped_lock lock(_mutex);
        _thirstLevel = std::clamp(value, 0.0f, 100.0f);
        stage = StageFromLevel(_thirstLevel);
        systemEnabled = _systemEnabled;
    }

    SyncHydrationStageSpell(stage, systemEnabled);
}

bool WaterNeedManager::IsSystemEnabled() const {
    std::scoped_lock lock(_mutex);
    return _systemEnabled;
}

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

int WaterNeedManager::GetPercent() const { return static_cast<int>(GetLevel()); }

int WaterNeedManager::GetStage() const { return StageFromLevel(GetLevel()); }

const char* WaterNeedManager::GetStageName() const { return STAGE_NAMES[GetStage()]; }

float WaterNeedManager::GetLastGameTime() const {
    std::scoped_lock lock(_mutex);
    return _lastGameTime;
}

void WaterNeedManager::SetLastGameTime(float value) {
    std::scoped_lock lock(_mutex);
    _lastGameTime = value;
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