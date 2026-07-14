#pragma once

namespace Settings {
    enum class Difficulty : int { Easy = 0, Medium = 1, Hard = 2, VeryHard = 3 };

    inline constexpr float DIFFICULTY_RATES[4] = {3.0f, 6.5f, 10.0f, 16.0f};
    inline constexpr float DRINK_AMOUNTS[4] = {65.0f, 55.0f, 45.0f, 35.0f};
    inline constexpr const char* DIFFICULTY_NAMES[4] = {"Easy", "Medium", "Hard", "Very Hard"};

    inline constexpr const char* DEFAULT_PLUGIN_NAME = "Tears of Kyne.esp";
    inline constexpr RE::FormID DEFAULT_FILLED_WATERSKIN_FORM_ID = 0x00000800;
    inline constexpr RE::FormID DEFAULT_EMPTY_WATERSKIN_FORM_ID = 0x00000804;

    inline constexpr float DEFAULT_WATER_SCAN_RADIUS = 100.0f;
    inline constexpr int DEFAULT_UPDATE_INTERVAL_SEC = 8;

    inline constexpr std::uint32_t DEFAULT_FILL_KEY = 0x13;
    inline constexpr std::uint32_t DEFAULT_TOGGLE_WIDGET_KEY = 0x23;

    inline constexpr int DEFAULT_HUD_X = 24;
    inline constexpr int DEFAULT_HUD_Y = 330;
    inline constexpr int DEFAULT_WIDGET_SCALE = 100;

    inline constexpr std::uint32_t SERIAL_UID = 'WTRN';
    inline constexpr std::uint32_t SERIAL_RECORD = 'THST';
    inline constexpr std::uint32_t SERIAL_VER = 3;

    inline constexpr float DEATH_HOURS_WITHOUT_WATER = 120.0f;

    inline Difficulty g_difficulty = Difficulty::Medium;
    inline float g_thirstRate = DIFFICULTY_RATES[1];
    inline float g_drinkAmount = DRINK_AMOUNTS[1];

    inline std::string g_pluginName = DEFAULT_PLUGIN_NAME;
    inline RE::FormID g_filledWaterskinFormID = DEFAULT_FILLED_WATERSKIN_FORM_ID;
    inline RE::FormID g_emptyWaterskinFormID = DEFAULT_EMPTY_WATERSKIN_FORM_ID;

    inline float g_waterScanRadius = DEFAULT_WATER_SCAN_RADIUS;
    inline int g_updateIntervalSec = DEFAULT_UPDATE_INTERVAL_SEC;

    inline std::uint32_t g_fillKey = DEFAULT_FILL_KEY;
    inline std::uint32_t g_toggleWidgetKey = DEFAULT_TOGGLE_WIDGET_KEY;

    inline int g_hudX = DEFAULT_HUD_X;
    inline int g_hudY = DEFAULT_HUD_Y;
    inline int g_widgetScale = DEFAULT_WIDGET_SCALE;
    inline bool g_hudVisible = true;
    inline bool g_widgetAutoHide = false;
    inline int g_widgetHoldSeconds = 5;
    inline bool g_pauseNeedsInJail = true;
    inline bool g_disableForVampire = true;
    inline bool g_deathByDehydration = false;
    inline bool g_enableTears = true;
    inline bool g_enableTearsWithSM = false;
    inline bool g_useFillPower = false;
    inline bool g_enableDirtyWater = false;
    inline float g_riskLow = 15.0f;
    inline float g_riskFoul = 90.0f;
    inline float g_bottleQuench = 50.0f;

    inline constexpr float DEFAULT_PERK_RATE_REDUCTION = 50.0f;
    inline bool g_enablePerkGate = false;
    inline std::string g_perkForms = "";
    inline float g_perkRateReduction = DEFAULT_PERK_RATE_REDUCTION;
    inline std::atomic<bool> g_perkFormsDirty = false;

    inline bool g_enableLogging = false;

    void ApplyDifficulty(Difficulty difficulty);
    int ParsePerkForms();
    [[nodiscard]] int GetParsedPerkCount();
    [[nodiscard]] int GetRequestedPerkCount();
    [[nodiscard]] bool PlayerHasGatePerk();
    [[nodiscard]] std::string GetKeyName(std::uint32_t scanCode);
    [[nodiscard]] bool TryParseKeyName(std::string value, std::uint32_t& out);
    void LoadFromINI();
    void SaveToINI();
}