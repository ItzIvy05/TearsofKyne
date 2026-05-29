#pragma once

#include <array>

class WaterNeedManager {
public:
    static WaterNeedManager* GetSingleton();

    // I dont know how to make it so i dont have to complile dll 
    // everytime to balance this {Missile Help}

    // Stage Breakpoints

    // Quenched     0 to 19.99
    // Sated        20 to 39.99
    // Thirsty      40 to 59.99
    // Parched      60 to 79.99
    // Dehydrated   80 to 100

    // Dehydration rate by difficulty

    // Easy = 3.0/ hour
    // Medium = 6.0/hour
    // Hard = 9.0/ hour 

    static constexpr float STAGE_BOUNDS[5] = { 0.0f, 20.0f, 40.0f, 60.0f, 80.0f };
    static constexpr const char* STAGE_NAMES[5] = { "Quenched", "Sated", "Thirsty", "Parched", "Dehydrated" };

    void InitializeFromSettings();
    void OnNewGame();
    void OnGameLoaded();
    void Tick();
    void Drink(float amount);
    void ForceSet(float value);

    [[nodiscard]] bool IsSystemEnabled() const;
    void SetSystemEnabled(bool enabled);

    [[nodiscard]] std::array<std::int32_t, 4> GetStoredBottleCounts() const;
    void SetStoredBottleCounts(const std::array<std::int32_t, 4>& counts);
    void ClearStoredBottleCounts();

    [[nodiscard]] float GetLevel() const;
    [[nodiscard]] int GetPercent() const;
    [[nodiscard]] int GetStage() const;
    [[nodiscard]] const char* GetStageName() const;

    [[nodiscard]] float GetLastGameTime() const;
    void SetLastGameTime(float value);

private:
    WaterNeedManager() = default;

    [[nodiscard]] int StageFromLevel(float level) const;
    [[nodiscard]] float GetCurrentGameTime() const;

    mutable std::mutex _mutex;
    float _thirstLevel = 0.0f;
    float _lastGameTime = -1.0f;
    bool _systemEnabled = true;
    std::array<std::int32_t, 4> _storedBottleCounts{};
};
