#pragma once

namespace Settings {
    enum class Difficulty : int {
        Easy = 0,
        Medium = 1,
        Hard = 2,
        VeryHard = 3
    };

    enum class WidgetStyle : int {
        Detailed = 0,
        Simple = 1,
        Nordic = 2,
        Ghost = 3
    };

    enum class AccessibilityTheme : int {
        Default = 0,
        Protanopia = 1,
        Deuteranopia = 2,
        Tritanopia = 3,
        Protanomaly = 4,
        Deuteranomaly = 5
    };

    inline constexpr float DIFFICULTY_RATES[4] = { 3.0f, 6.5f, 10.0f, 16.0f };
    inline constexpr float DRINK_AMOUNTS[4] = { 65.0f, 55.0f, 45.0f, 35.0f };
    inline constexpr float FILL_BONUSES[4] = { 20.0f, 15.0f, 10.0f, 5.0f };
    inline constexpr const char* DIFFICULTY_NAMES[4] = { "Easy", "Medium", "Hard", "Very Hard" };
    inline constexpr const char* WIDGET_STYLE_NAMES[4] = { "Default", "Simple", "Nordic", "Ghost" };
    inline constexpr const char* ACCESSIBILITY_THEME_NAMES[6] = { "Default", "Protanopia", "Deuteranopia", "Tritanopia", "Protanomaly", "Deuteranomaly" };

    inline constexpr const char* DEFAULT_PLUGIN_NAME = "Tears of Kyne.esp";
    inline constexpr RE::FormID DEFAULT_FILLED_WATERSKIN_FORM_ID = 0x00000800;
    inline constexpr RE::FormID DEFAULT_EMPTY_WATERSKIN_FORM_ID = 0x00000804;

    inline constexpr float DEFAULT_WATER_SCAN_RADIUS = 100.0f;
    inline constexpr int DEFAULT_UPDATE_INTERVAL_SEC = 8;

    inline constexpr std::uint32_t DEFAULT_FILL_KEY = 0x13;
    inline constexpr std::uint32_t DEFAULT_DRINK_KEY = 0x22;
    inline constexpr std::uint32_t DEFAULT_MENU_KEY = 0x44;
    inline constexpr std::uint32_t DEFAULT_TOGGLE_HUD_KEY = 0x23;

    inline constexpr int DEFAULT_HUD_X = 24;
    inline constexpr int DEFAULT_HUD_Y = 28;
    inline constexpr int DEFAULT_MENU_X = -1;
    inline constexpr int DEFAULT_MENU_Y = -1;
    inline constexpr int DEFAULT_WIDGET_SCALE = 100;

    inline constexpr std::uint32_t SERIAL_UID = 'WTRN';
    inline constexpr std::uint32_t SERIAL_RECORD = 'THST';
    inline constexpr std::uint32_t SERIAL_VER = 2;
    inline constexpr const char* UI_VIEW_PATH = "TearsOfKyne/index.html";

    inline Difficulty g_difficulty = Difficulty::Medium;
    inline float g_thirstRate = DIFFICULTY_RATES[1];
    inline float g_drinkAmount = DRINK_AMOUNTS[1];
    inline float g_fillBonus = FILL_BONUSES[1];

    inline WidgetStyle g_widgetStyle = WidgetStyle::Detailed;
    inline AccessibilityTheme g_accessibilityTheme = AccessibilityTheme::Default;

    inline std::string g_pluginName = DEFAULT_PLUGIN_NAME;
    inline RE::FormID g_filledWaterskinFormID = DEFAULT_FILLED_WATERSKIN_FORM_ID;
    inline RE::FormID g_emptyWaterskinFormID = DEFAULT_EMPTY_WATERSKIN_FORM_ID;

    inline float g_waterScanRadius = DEFAULT_WATER_SCAN_RADIUS;
    inline int g_updateIntervalSec = DEFAULT_UPDATE_INTERVAL_SEC;

    inline std::uint32_t g_fillKey = DEFAULT_FILL_KEY;
    inline std::uint32_t g_drinkKey = DEFAULT_DRINK_KEY;
    inline std::uint32_t g_menuKey = DEFAULT_MENU_KEY;
    inline std::uint32_t g_toggleHUDKey = DEFAULT_TOGGLE_HUD_KEY;

    inline int g_hudX = DEFAULT_HUD_X;
    inline int g_hudY = DEFAULT_HUD_Y;
    inline int g_menuX = DEFAULT_MENU_X;
    inline int g_menuY = DEFAULT_MENU_Y;
    inline int g_widgetScale = DEFAULT_WIDGET_SCALE;
    inline bool g_hudVisible = true;

    void ApplyDifficulty(Difficulty difficulty);
    void ApplyAccessibilityTheme(AccessibilityTheme theme);
    [[nodiscard]] std::string GetKeyName(std::uint32_t scanCode);
    [[nodiscard]] bool TryParseKeyName(std::string value, std::uint32_t& out);
    void LoadFromINI();
    void SaveToINI();
}
