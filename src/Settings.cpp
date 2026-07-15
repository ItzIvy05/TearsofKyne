#include "Settings.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace Settings {
    namespace {
        struct KeyNameEntry {
            std::uint32_t code;
            const char* name;
        };

        constexpr KeyNameEntry KEY_NAMES[] = {
            {0x01, "ESC"},   {0x02, "1"},         {0x03, "2"},     {0x04, "3"},     {0x05, "4"},     {0x06, "5"},
            {0x07, "6"},     {0x08, "7"},         {0x09, "8"},     {0x0A, "9"},     {0x0B, "0"},     {0x0C, "-"},
            {0x0D, "="},     {0x0E, "BACKSPACE"}, {0x0F, "TAB"},   {0x10, "Q"},     {0x11, "W"},     {0x12, "E"},
            {0x13, "R"},     {0x14, "T"},         {0x15, "Y"},     {0x16, "U"},     {0x17, "I"},     {0x18, "O"},
            {0x19, "P"},     {0x1A, "["},         {0x1B, "]"},     {0x1C, "ENTER"}, {0x1D, "LCTRL"}, {0x1E, "A"},
            {0x1F, "S"},     {0x20, "D"},         {0x21, "F"},     {0x22, "G"},     {0x23, "H"},     {0x24, "J"},
            {0x25, "K"},     {0x26, "L"},         {0x27, ";"},     {0x28, "'"},     {0x29, "`"},     {0x2A, "LSHIFT"},
            {0x2B, "\\"},    {0x2C, "Z"},         {0x2D, "X"},     {0x2E, "C"},     {0x2F, "V"},     {0x30, "B"},
            {0x31, "N"},     {0x32, "M"},         {0x33, ","},     {0x34, "."},     {0x35, "/"},     {0x36, "RSHIFT"},
            {0x37, "NUM*"},  {0x38, "LALT"},      {0x39, "SPACE"}, {0x3A, "CAPS"},  {0x3B, "F1"},    {0x3C, "F2"},
            {0x3D, "F3"},    {0x3E, "F4"},        {0x3F, "F5"},    {0x40, "F6"},    {0x41, "F7"},    {0x42, "F8"},
            {0x43, "F9"},    {0x44, "F10"},       {0x57, "F11"},   {0x58, "F12"},   {0x47, "NUM7"},  {0x48, "NUM8"},
            {0x49, "NUM9"},  {0x4A, "NUM-"},      {0x4B, "NUM4"},  {0x4C, "NUM5"},  {0x4D, "NUM6"},  {0x4E, "NUM+"},
            {0x4F, "NUM1"},  {0x50, "NUM2"},      {0x51, "NUM3"},  {0x52, "NUM0"},  {0x53, "NUM."},  {0x9C, "NUMENTER"},
            {0x9D, "RCTRL"}, {0xB5, "NUM/"},      {0xB8, "RALT"},  {0xC7, "HOME"},  {0xC8, "UP"},    {0xC9, "PGUP"},
            {0xCB, "LEFT"},  {0xCD, "RIGHT"},     {0xCF, "END"},   {0xD0, "DOWN"},  {0xD1, "PGDN"},  {0xD2, "INS"},
            {0xD3, "DEL"}};

        void Trim(std::string& value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                value.clear();
                return;
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            value = value.substr(first, last - first + 1);
        }

        std::string UpperCopy(std::string value) {
            for (char& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return value;
        }

        bool TryParseUInt(std::string value, std::uint32_t& out) {
            Trim(value);
            if (value.empty()) return false;
            try {
                std::size_t index = 0;
                const auto parsed = std::stoul(value, &index, 0);
                if (index != value.size()) return false;
                out = static_cast<std::uint32_t>(parsed);
                return true;
            } catch (...) {
                return false;
            }
        }

        bool ParseBool(std::string value, bool fallback) {
            Trim(value);
            value = UpperCopy(value);
            if (value == "1" || value == "TRUE" || value == "YES" || value == "ON") return true;
            if (value == "0" || value == "FALSE" || value == "NO" || value == "OFF") return false;
            return fallback;
        }

        std::string GetINIPath() {
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

        void ResetDefaults() {
            ApplyDifficulty(Difficulty::Medium);
            g_pluginName = DEFAULT_PLUGIN_NAME;
            g_filledWaterskinFormID = DEFAULT_FILLED_WATERSKIN_FORM_ID;
            g_emptyWaterskinFormID = DEFAULT_EMPTY_WATERSKIN_FORM_ID;
            g_waterScanRadius = DEFAULT_WATER_SCAN_RADIUS;
            g_updateIntervalSec = DEFAULT_UPDATE_INTERVAL_SEC;
            g_fillKey = DEFAULT_FILL_KEY;
            g_toggleWidgetKey = DEFAULT_TOGGLE_WIDGET_KEY;
            g_hudX = DEFAULT_HUD_X;
            g_hudY = DEFAULT_HUD_Y;
            g_widgetScale = DEFAULT_WIDGET_SCALE;
            g_hudVisible = true;
            g_widgetAutoHide = false;
            g_widgetHoldSeconds = 5;
            g_pauseNeedsInJail = true;
            g_disableForVampire = true;
            g_deathByDehydration = false;
            g_enableTears = true;
            g_enableTearsWithSM = false;
            g_useFillPower = false;
            g_enableDirtyWater = false;
            g_riskLow = 15.0f;
            g_riskFoul = 90.0f;
            g_bottleQuench = 50.0f;
            g_reuseBottles = true;
            g_enablePerkGate = false;
            g_perkForms = "";
            g_perkRateReduction = DEFAULT_PERK_RATE_REDUCTION;
            g_enableLogging = false;
        }
    }

    void ApplyDifficulty(Difficulty difficulty) {
        const auto index = static_cast<int>(difficulty);
        g_difficulty = difficulty;
        g_thirstRate = DIFFICULTY_RATES[index];
        g_drinkAmount = DRINK_AMOUNTS[index];
    }

    namespace {
        std::vector<RE::BGSPerk*> g_parsedPerks;
        int g_requestedPerkCount = 0;
    }

    int ParsePerkForms() {
        g_parsedPerks.clear();
        g_requestedPerkCount = 1;

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return 0;
        }

        std::string entry = g_perkForms;
        if (const size_t comma = entry.find(','); comma != std::string::npos) {
            entry = entry.substr(0, comma);
        }

        const size_t a = entry.find_first_not_of(" \t");
        if (a == std::string::npos) {
            return 0;
        }
        const size_t b = entry.find_last_not_of(" \t");
        entry = entry.substr(a, b - a + 1);
        if (entry.empty()) {
            return 0;
        }

        const size_t bar = entry.find('|');
        if (bar == std::string::npos) {
            return 0;
        }

        const std::string plugin = entry.substr(0, bar);
        const std::string idStr = entry.substr(bar + 1);
        if (plugin.empty() || idStr.empty()) {
            return 0;
        }

        RE::FormID localID = 0;
        try {
            localID = static_cast<RE::FormID>(std::stoul(idStr, nullptr, 16));
        } catch (...) {
            return 0;
        }

        if (auto* form = dataHandler->LookupForm(localID, plugin)) {
            if (auto* perk = form->As<RE::BGSPerk>()) {
                g_parsedPerks.push_back(perk);
            }
        }

        logger::info("[Settings] Perk gate: {}/{} perks parsed.", static_cast<int>(g_parsedPerks.size()),
                     g_requestedPerkCount);
        return static_cast<int>(g_parsedPerks.size());
    }

    int GetParsedPerkCount() { return static_cast<int>(g_parsedPerks.size()); }
    int GetRequestedPerkCount() { return g_requestedPerkCount; }

    bool PlayerHasGatePerk() {
        if (g_parsedPerks.empty()) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;
        for (auto* perk : g_parsedPerks) {
            if (perk && player->HasPerk(perk)) return true;
        }
        return false;
    }

    std::string GetKeyName(std::uint32_t scanCode) {
        if (scanCode == 0) return "Unbound";
        for (const auto& entry : KEY_NAMES) {
            if (entry.code == scanCode) return entry.name;
        }
        return std::format("0x{:02X}", scanCode);
    }

    bool TryParseKeyName(std::string value, std::uint32_t& out) {
        Trim(value);
        if (value.empty()) return false;
        if (TryParseUInt(value, out)) return true;
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

    void LoadFromINI() {
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
            if (const auto comment = line.find(';'); comment != std::string::npos) line.erase(comment);
            Trim(line);
            if (line.empty() || line.front() == '[') continue;

            const auto equals = line.find('=');
            if (equals == std::string::npos) continue;

            std::string key = line.substr(0, equals);
            std::string value = line.substr(equals + 1);
            Trim(key);
            Trim(value);

            std::uint32_t parsedKey = 0;

            if (key == "FillKey") {
                if (TryParseKeyName(value, parsedKey)) g_fillKey = parsedKey;
            } else if (key == "ToggleWidgetKey") {
                if (value == "0" || UpperCopy(value) == "UNBOUND") {
                    g_toggleWidgetKey = 0;
                } else if (TryParseKeyName(value, parsedKey)) {
                    g_toggleWidgetKey = parsedKey;
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
            } else if (key == "WidgetScale") {
                try {
                    g_widgetScale = std::clamp(std::stoi(value), 10, 150);
                } catch (...) {
                }
            } else if (key == "bHudVisible") {
                g_hudVisible = ParseBool(value, true);
            } else if (key == "bWidgetAutoHide") {
                g_widgetAutoHide = ParseBool(value, false);
            } else if (key == "WidgetHoldSeconds") {
                try {
                    g_widgetHoldSeconds = std::clamp(std::stoi(value), 3, 60);
                } catch (...) {
                }
            } else if (key == "bPauseNeedsInJail") {
                g_pauseNeedsInJail = ParseBool(value, true);
            } else if (key == "bDisableForVampire") {
                g_disableForVampire = ParseBool(value, true);
            } else if (key == "bDeathByDehydration") {
                g_deathByDehydration = ParseBool(value, false);
            } else if (key == "bEnableTears") {
                g_enableTears = ParseBool(value, true);
            } else if (key == "bEnableTearsWithSM") {
                g_enableTearsWithSM = ParseBool(value, false);
            } else if (key == "bUseFillPower") {
                g_useFillPower = ParseBool(value, false);
            } else if (key == "bEnableDirtyWater") {
                g_enableDirtyWater = ParseBool(value, false);
            } else if (key == "fRiskLow") {
                try {
                    g_riskLow = std::clamp(std::stof(value), 0.0f, 100.0f);
                } catch (...) {
                }
            } else if (key == "fRiskFoul") {
                try {
                    g_riskFoul = std::clamp(std::stof(value), 0.0f, 100.0f);
                } catch (...) {
                }
            } else if (key == "fBottleQuench") {
                try {
                    g_bottleQuench = std::clamp(std::stof(value), 15.0f, 75.0f);
                } catch (...) {
                }
            } else if (key == "bReuseBottles") {
                g_reuseBottles = ParseBool(value, true);
            } else if (key == "bEnablePerkGate") {
                g_enablePerkGate = ParseBool(value, false);
            } else if (key == "sPerkForms") {
                g_perkForms = value;
            } else if (key == "fPerkRateReduction") {
                try {
                    g_perkRateReduction = std::clamp(std::stof(value), 15.0f, 75.0f);
                } catch (...) {
                }
            } else if (key == "bEnableLogging") {
                g_enableLogging = ParseBool(value, false);
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
            }
        }

        if (g_enablePerkGate && g_difficulty != Difficulty::Hard && g_difficulty != Difficulty::VeryHard) {
            ApplyDifficulty(Difficulty::Hard);
        }

        ParsePerkForms();

        spdlog::set_level(g_enableLogging ? spdlog::level::info : spdlog::level::off);

        logger::info(
            "[Settings] Loaded. FillKey={} ToggleWidgetKey={} HudX={} HudY={} WidgetScale={} HUDVisible={} "
            "Difficulty={}",
            GetKeyName(g_fillKey), GetKeyName(g_toggleWidgetKey), g_hudX, g_hudY, g_widgetScale, g_hudVisible ? 1 : 0,
            DIFFICULTY_NAMES[static_cast<int>(g_difficulty)]);
    }

    void SaveToINI() {
        const auto path = GetINIPath();
        if (path.empty()) return;

        std::vector<std::pair<std::string, std::string>> values = {
            {"FillKey", GetKeyName(g_fillKey)},
            {"ToggleWidgetKey", g_toggleWidgetKey == 0 ? "0" : GetKeyName(g_toggleWidgetKey)},
            {"HudX", std::to_string(g_hudX)},
            {"HudY", std::to_string(g_hudY)},
            {"WidgetScale", std::to_string(g_widgetScale)},
            {"bHudVisible", g_hudVisible ? "1" : "0"},
            {"bWidgetAutoHide", g_widgetAutoHide ? "1" : "0"},
            {"WidgetHoldSeconds", std::to_string(g_widgetHoldSeconds)},
            {"bPauseNeedsInJail", g_pauseNeedsInJail ? "1" : "0"},
            {"bDisableForVampire", g_disableForVampire ? "1" : "0"},
            {"bDeathByDehydration", g_deathByDehydration ? "1" : "0"},
            {"bEnableTears", g_enableTears ? "1" : "0"},
            {"bEnableTearsWithSM", g_enableTearsWithSM ? "1" : "0"},
            {"bUseFillPower", g_useFillPower ? "1" : "0"},
            {"bEnableDirtyWater", g_enableDirtyWater ? "1" : "0"},
            {"fRiskLow", std::format("{}", g_riskLow)},
            {"fRiskFoul", std::format("{}", g_riskFoul)},
            {"fBottleQuench", std::format("{}", g_bottleQuench)},
            {"bReuseBottles", g_reuseBottles ? "1" : "0"},
            {"bEnablePerkGate", g_enablePerkGate ? "1" : "0"},
            {"sPerkForms", g_perkForms},
            {"fPerkRateReduction", std::format("{}", g_perkRateReduction)},
            {"bEnableLogging", g_enableLogging ? "1" : "0"},
            {"Difficulty", std::to_string(static_cast<int>(g_difficulty))},
        };

        std::vector<std::string> lines;
        bool hadFile = false;
        {
            std::ifstream in(path);
            if (in.is_open()) {
                hadFile = true;
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    lines.push_back(line);
                }
            }
        }

        std::vector<bool> written(values.size(), false);

        for (auto& line : lines) {
            const size_t firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos) continue;
            const char c = line[firstNonSpace];
            if (c == ';' || c == '#' || c == '[') continue;

            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(firstNonSpace, eq - firstNonSpace);
            const size_t keyEnd = key.find_last_not_of(" \t");
            if (keyEnd != std::string::npos) key = key.substr(0, keyEnd + 1);

            for (size_t i = 0; i < values.size(); ++i) {
                if (!written[i] && key == values[i].first) {
                    line = values[i].first + "=" + values[i].second;
                    written[i] = true;
                    break;
                }
            }
        }

        const std::string logComment = "; bEnableLogging=1 writes a debug log; 0 (default)";
        for (size_t i = 0; i < lines.size(); ++i) {
            const size_t firstNonSpace = lines[i].find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos) continue;
            if (lines[i].compare(firstNonSpace, 14, "bEnableLogging") == 0) {
                if (i == 0 || lines[i - 1] != logComment) {
                    lines.insert(lines.begin() + i, logComment);
                }
                break;
            }
        }

        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) {
            logger::error("[Settings] Could not write INI to '{}'.", path);
            return;
        }

        if (!hadFile) {
            out << "[Tears of Kyne]\n";
        }

        for (const auto& line : lines) {
            out << line << "\n";
        }

        bool anyMissing = false;
        for (size_t i = 0; i < values.size(); ++i) {
            if (!written[i]) {
                anyMissing = true;
                break;
            }
        }
        if (anyMissing) {
            for (size_t i = 0; i < values.size(); ++i) {
                if (!written[i]) {
                    if (values[i].first == "bEnableLogging") {
                        out << logComment << "\n";
                    }
                    out << values[i].first << "=" << values[i].second << "\n";
                }
            }
        }

        logger::info("[Settings] Wrote INI to '{}'.", path);
    }
}