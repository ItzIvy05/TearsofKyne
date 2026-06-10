#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <Windows.h>

namespace Settings {
    namespace {
        struct KeyNameEntry {
            std::uint32_t code;
            const char* name;
        };

        constexpr KeyNameEntry KEY_NAMES[] = {
            { 0x01, "ESC" },
            { 0x02, "1" }, { 0x03, "2" }, { 0x04, "3" }, { 0x05, "4" }, { 0x06, "5" },
            { 0x07, "6" }, { 0x08, "7" }, { 0x09, "8" }, { 0x0A, "9" }, { 0x0B, "0" },
            { 0x0C, "-" }, { 0x0D, "=" }, { 0x0E, "BACKSPACE" }, { 0x0F, "TAB" },
            { 0x10, "Q" }, { 0x11, "W" }, { 0x12, "E" }, { 0x13, "R" }, { 0x14, "T" },
            { 0x15, "Y" }, { 0x16, "U" }, { 0x17, "I" }, { 0x18, "O" }, { 0x19, "P" },
            { 0x1A, "[" }, { 0x1B, "]" }, { 0x1C, "ENTER" }, { 0x1D, "LCTRL" },
            { 0x1E, "A" }, { 0x1F, "S" }, { 0x20, "D" }, { 0x21, "F" }, { 0x22, "G" },
            { 0x23, "H" }, { 0x24, "J" }, { 0x25, "K" }, { 0x26, "L" }, { 0x27, ";" },
            { 0x28, "'" }, { 0x29, "`" }, { 0x2A, "LSHIFT" }, { 0x2B, "\\" },
            { 0x2C, "Z" }, { 0x2D, "X" }, { 0x2E, "C" }, { 0x2F, "V" }, { 0x30, "B" },
            { 0x31, "N" }, { 0x32, "M" }, { 0x33, "," }, { 0x34, "." }, { 0x35, "/" },
            { 0x36, "RSHIFT" }, { 0x37, "NUM*" }, { 0x38, "LALT" }, { 0x39, "SPACE" },
            { 0x3A, "CAPS" },
            { 0x3B, "F1" }, { 0x3C, "F2" }, { 0x3D, "F3" }, { 0x3E, "F4" },
            { 0x3F, "F5" }, { 0x40, "F6" }, { 0x41, "F7" }, { 0x42, "F8" },
            { 0x43, "F9" }, { 0x44, "F10" }, { 0x57, "F11" }, { 0x58, "F12" },
            { 0x47, "NUM7" }, { 0x48, "NUM8" }, { 0x49, "NUM9" }, { 0x4A, "NUM-" },
            { 0x4B, "NUM4" }, { 0x4C, "NUM5" }, { 0x4D, "NUM6" }, { 0x4E, "NUM+" },
            { 0x4F, "NUM1" }, { 0x50, "NUM2" }, { 0x51, "NUM3" }, { 0x52, "NUM0" },
            { 0x53, "NUM." },
            { 0x9C, "NUMENTER" }, { 0x9D, "RCTRL" }, { 0xB5, "NUM/" }, { 0xB8, "RALT" },
            { 0xC7, "HOME" }, { 0xC8, "UP" }, { 0xC9, "PGUP" }, { 0xCB, "LEFT" },
            { 0xCD, "RIGHT" }, { 0xCF, "END" }, { 0xD0, "DOWN" }, { 0xD1, "PGDN" },
            { 0xD2, "INS" }, { 0xD3, "DEL" }
        };

        void Trim(std::string& value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                value.clear();
                return;
            }

            const auto last = value.find_last_not_of(" \t\r\n");
            value = value.substr(first, last - first + 1);
        }

        std::string UpperCopy(std::string value)
        {
            for (char& c : value) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return value;
        }

        bool TryParseUInt(std::string value, std::uint32_t& out)
        {
            Trim(value);
            if (value.empty()) {
                return false;
            }

            try {
                std::size_t index = 0;
                const auto parsed = std::stoul(value, &index, 0);
                if (index != value.size()) {
                    return false;
                }
                out = static_cast<std::uint32_t>(parsed);
                return true;
            } catch (...) {
                return false;
            }
        }

        bool ParseBool(std::string value, bool fallback)
        {
            Trim(value);
            value = UpperCopy(value);

            if (value == "1" || value == "TRUE" || value == "YES" || value == "ON") {
                return true;
            }
            if (value == "0" || value == "FALSE" || value == "NO" || value == "OFF") {
                return false;
            }
            return fallback;
        }

        std::string GetINIPath()
        {
            wchar_t modulePath[MAX_PATH]{};
            const HMODULE module = GetModuleHandleW(L"Tears of Kyne.dll");
            if (!module) {
                logger::error("[Settings] Could not get module handle for Tears of Kyne.dll");
                return {};
            }

            const DWORD length = GetModuleFileNameW(module, modulePath, MAX_PATH);
            if (length == 0 || length == MAX_PATH) {
                logger::error("[Settings] Could not get module file path");
                return {};
            }

            return (std::filesystem::path(modulePath).parent_path() / L"TearsOfKyne.ini").string();
        }

        void ResetDefaults()
        {
            ApplyDifficulty(Difficulty::Medium);
            g_widgetStyle = WidgetStyle::Detailed;
            g_accessibilityTheme = AccessibilityTheme::Default;
            g_pluginName = DEFAULT_PLUGIN_NAME;
            g_filledWaterskinFormID = DEFAULT_FILLED_WATERSKIN_FORM_ID;
            g_emptyWaterskinFormID = DEFAULT_EMPTY_WATERSKIN_FORM_ID;
            g_waterScanRadius = DEFAULT_WATER_SCAN_RADIUS;
            g_updateIntervalSec = DEFAULT_UPDATE_INTERVAL_SEC;
            g_fillKey = DEFAULT_FILL_KEY;
            g_drinkKey = DEFAULT_DRINK_KEY;
            g_menuKey = DEFAULT_MENU_KEY;
            g_toggleHUDKey = DEFAULT_TOGGLE_HUD_KEY;
            g_hudX = DEFAULT_HUD_X;
            g_hudY = DEFAULT_HUD_Y;
            g_menuX = DEFAULT_MENU_X;
            g_menuY = DEFAULT_MENU_Y;
            g_widgetScale = DEFAULT_WIDGET_SCALE;
            g_hudVisible = true;
        }
    }

    void ApplyDifficulty(Difficulty difficulty)
    {
        const auto index = static_cast<int>(difficulty);
        g_difficulty = difficulty;
        g_thirstRate = DIFFICULTY_RATES[index];
        g_drinkAmount = DRINK_AMOUNTS[index];
        g_fillBonus = FILL_BONUSES[index];
    }

    void ApplyAccessibilityTheme(AccessibilityTheme theme)
    {
        const auto index = std::clamp(static_cast<int>(theme), 0, 5);
        g_accessibilityTheme = static_cast<AccessibilityTheme>(index);
    }

    std::string GetKeyName(std::uint32_t scanCode)
    {
        for (const auto& entry : KEY_NAMES) {
            if (entry.code == scanCode) {
                return entry.name;
            }
        }

        return std::format("0x{:02X}", scanCode);
    }

    bool TryParseKeyName(std::string value, std::uint32_t& out)
    {
        Trim(value);
        if (value.empty()) {
            return false;
        }

        if (TryParseUInt(value, out)) {
            return true;
        }

        value = UpperCopy(value);
        if (value == "NUMPAD+") value = "NUM+";
        if (value == "NUMPAD-") value = "NUM-";
        if (value == "NUMPAD/") value = "NUM/";
        if (value == "NUMPAD*") value = "NUM*";
        if (value == "NUMPADENTER") value = "NUMENTER";

        for (const auto& entry : KEY_NAMES) {
            if (value == entry.name) {
                out = entry.code;
                return true;
            }
        }

        return false;
    }

    void LoadFromINI()
    {
        ResetDefaults();

        const auto path = GetINIPath();
        if (path.empty()) {
            logger::warn("[Settings] Could not determine INI path. Using defaults.");
            return;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            logger::info("[Settings] INI not found at '{}'. Creating defaults.", path);
            SaveToINI();
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (const auto comment = line.find(';'); comment != std::string::npos) {
                line.erase(comment);
            }

            Trim(line);
            if (line.empty() || line.front() == '[') {
                continue;
            }

            const auto equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, equals);
            std::string value = line.substr(equals + 1);
            Trim(key);
            Trim(value);

            std::uint32_t parsedKey = 0;

            if (key == "FillKey") {
                if (TryParseKeyName(value, parsedKey)) {
                    g_fillKey = parsedKey;
                }
            } else if (key == "DrinkKey") {
                if (TryParseKeyName(value, parsedKey)) {
                    g_drinkKey = parsedKey;
                }
            } else if (key == "MenuKey") {
                if (TryParseKeyName(value, parsedKey)) {
                    g_menuKey = parsedKey;
                }
            } else if (key == "ToggleHUDKey") {
                if (TryParseKeyName(value, parsedKey)) {
                    g_toggleHUDKey = parsedKey;
                }
            } else if (key == "HudX") {
                try {
                    g_hudX = std::max(0, std::stoi(value));
                } catch (...) {
                }
            } else if (key == "HudY") {
                try {
                    g_hudY = std::max(0, std::stoi(value));
                } catch (...) {
                }
            } else if (key == "MenuX") {
                try {
                    g_menuX = std::stoi(value);
                } catch (...) {
                }
            } else if (key == "MenuY") {
                try {
                    g_menuY = std::stoi(value);
                } catch (...) {
                }
            } else if (key == "WidgetScale") {
                try {
                    g_widgetScale = std::clamp(std::stoi(value), 50, 150);
                } catch (...) {
                }
            } else if (key == "bHudVisible") {
                g_hudVisible = ParseBool(value, true);
            } else if (key == "WidgetStyle") {
                const auto upper = UpperCopy(value);
                if (upper == "1" || upper == "SIMPLE") {
                    g_widgetStyle = WidgetStyle::Simple;
                } else if (upper == "2" || upper == "NORDIC") {
                    g_widgetStyle = WidgetStyle::Nordic;
                } else if (upper == "3" || upper == "GHOST") {
                    g_widgetStyle = WidgetStyle::Ghost;
                } else {
                    g_widgetStyle = WidgetStyle::Detailed;
                }
            } else if (key == "Difficulty") {
                const auto upper = UpperCopy(value);
                if (upper == "0" || upper == "EASY") {
                    ApplyDifficulty(Difficulty::Easy);
                } else if (upper == "2" || upper == "HARD") {
                    ApplyDifficulty(Difficulty::Hard);
                } else if (upper == "3" || upper == "VERYHARD" || upper == "VERY HARD") {
                    ApplyDifficulty(Difficulty::VeryHard);
                } else {
                    ApplyDifficulty(Difficulty::Medium);
                }
            } else if (key == "AccessibilityTheme") {
                const auto upper = UpperCopy(value);
                if (upper == "1" || upper == "PROTANOPIA") {
                    ApplyAccessibilityTheme(AccessibilityTheme::Protanopia);
                } else if (upper == "2" || upper == "DEUTERANOPIA") {
                    ApplyAccessibilityTheme(AccessibilityTheme::Deuteranopia);
                } else if (upper == "3" || upper == "TRITANOPIA") {
                    ApplyAccessibilityTheme(AccessibilityTheme::Tritanopia);
                } else if (upper == "4" || upper == "PROTANOMALY") {
                    ApplyAccessibilityTheme(AccessibilityTheme::Protanomaly);
                } else if (upper == "5" || upper == "DEUTERANOMALY") {
                    ApplyAccessibilityTheme(AccessibilityTheme::Deuteranomaly);
                } else {
                    ApplyAccessibilityTheme(AccessibilityTheme::Default);
                }
            }
        }

        logger::info(
            "[Settings] Loaded. FillKey={} MenuKey={} ToggleHUDKey={} HudX={} HudY={} MenuX={} MenuY={} WidgetScale={} HUDVisible={} WidgetStyle={} Difficulty={} AccessibilityTheme={}",
            GetKeyName(g_fillKey),
            GetKeyName(g_menuKey),
            GetKeyName(g_toggleHUDKey),
            g_hudX,
            g_hudY,
            g_menuX,
            g_menuY,
            g_widgetScale,
            g_hudVisible ? 1 : 0,
            WIDGET_STYLE_NAMES[std::clamp(static_cast<int>(g_widgetStyle), 0, 3)],
            DIFFICULTY_NAMES[static_cast<int>(g_difficulty)],
            ACCESSIBILITY_THEME_NAMES[static_cast<int>(g_accessibilityTheme)]);
    }

    void SaveToINI()
    {
        const auto path = GetINIPath();
        if (path.empty()) {
            return;
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) {
            logger::error("[Settings] Could not write INI to '{}'.", path);
            return;
        }

        file << "[Tears of Kyne]\n\n"
             << "FillKey=" << GetKeyName(g_fillKey) << "\n"
             << "DrinkKey=" << GetKeyName(g_drinkKey) << "\n"
             << "MenuKey=" << GetKeyName(g_menuKey) << "\n"
             << "ToggleHUDKey=" << GetKeyName(g_toggleHUDKey) << "\n\n"
             << "HudX=" << g_hudX << "\n"
             << "HudY=" << g_hudY << "\n"
             << "MenuX=" << g_menuX << "\n"
             << "MenuY=" << g_menuY << "\n"
             << "WidgetScale=" << g_widgetScale << "\n"
             << "bHudVisible=" << (g_hudVisible ? 1 : 0) << "\n"
             << "WidgetStyle=" << static_cast<int>(g_widgetStyle) << "\n"
             << "Difficulty=" << static_cast<int>(g_difficulty) << "\n"
             << "AccessibilityTheme=" << static_cast<int>(g_accessibilityTheme) << "\n";

        logger::info("[Settings] Wrote INI to '{}'.", path);
    }
}
