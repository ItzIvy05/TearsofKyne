#include "Settings.h"

#include <Windows.h>

#include <algorithm>
#include <climits>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#include <ClibUtil/hotkeys.hpp>

namespace Settings {
    namespace {
        bool TryParseUInt(std::string value, std::uint32_t& out) {
            clib_util::string::trim(value);
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
            clib_util::string::trim(value);
            value = clib_util::string::toupper(value);
            if (value == "1" || value == "TRUE" || value == "YES" || value == "ON") return true;
            if (value == "0" || value == "FALSE" || value == "NO" || value == "OFF") return false;
            return fallback;
        }

        template <class T>
        void ReadNumber(const std::string& value, T& out, T min, T max) {
            try {
                out = std::clamp(clib_util::string::to_num<T>(value), min, max);
            } catch (...) {
            }
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
            g_fillMethod = FillMethod::Hotkey;
            g_enableDirtyWater = false;
            g_riskLow = 15.0f;
            g_riskFoul = 90.0f;
            g_bottleQuench = 50.0f;
            g_reuseBottles = true;
            g_enablePerkGate = false;
            g_perkForms = "";
            g_perkRateReduction = DEFAULT_PERK_RATE_REDUCTION;
            g_useKynesEmbraceGlobal = false;
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

        if (g_perkForms.empty()) {
            return 0;
        }

        auto entry = clib_util::string::split(g_perkForms, ",").front();
        clib_util::string::trim(entry);

        const auto parts = clib_util::string::split(entry, "|");
        if (parts.size() < 2 || parts[0].empty() || parts[1].empty()) {
            return 0;
        }

        RE::FormID localID = 0;
        try {
            localID = clib_util::string::to_num<RE::FormID>(parts[1], true);
        } catch (...) {
            return 0;
        }

        if (auto* perk = dataHandler->LookupForm<RE::BGSPerk>(localID, parts[0])) {
            g_parsedPerks.push_back(perk);
        }

        logger::info("[Settings] Perk gate: {}/{} perks parsed.", static_cast<int>(g_parsedPerks.size()), g_requestedPerkCount);
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
        try {
            return clib_util::string::toupper(clib_util::hotkeys::details::GetNameByKey(scanCode));
        } catch (const std::range_error&) {
            return std::format("0x{:02X}", scanCode);
        }
    }

    bool TryParseKeyName(std::string value, std::uint32_t& out) {
        clib_util::string::trim(value);
        if (value.empty()) return false;
        try {
            out = clib_util::hotkeys::details::GetKeyByName(clib_util::string::tolower(value));
            return true;
        } catch (const std::range_error&) {
            return TryParseUInt(value, out);
        }
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

        bool hasFillMethod = false;
        bool legacyFillPower = false;

        std::string line;
        while (std::getline(file, line)) {
            if (const auto comment = line.find(';'); comment != std::string::npos) line.erase(comment);
            clib_util::string::trim(line);
            if (line.empty() || line.front() == '[') continue;

            const auto equals = line.find('=');
            if (equals == std::string::npos) continue;

            std::string key = line.substr(0, equals);
            std::string value = line.substr(equals + 1);
            clib_util::string::trim(key);
            clib_util::string::trim(value);

            std::uint32_t parsedKey = 0;

            if (key == "FillKey") {
                if (TryParseKeyName(value, parsedKey)) g_fillKey = parsedKey;
            } else if (key == "ToggleWidgetKey") {
                if (value == "0" || clib_util::string::toupper(value) == "UNBOUND") {
                    g_toggleWidgetKey = 0;
                } else if (TryParseKeyName(value, parsedKey)) {
                    g_toggleWidgetKey = parsedKey;
                }
            } else if (key == "HudX") {
                ReadNumber(value, g_hudX, 0, INT_MAX);
            } else if (key == "HudY") {
                ReadNumber(value, g_hudY, 0, INT_MAX);
            } else if (key == "WidgetScale") {
                ReadNumber(value, g_widgetScale, 10, 150);
            } else if (key == "bHudVisible") {
                g_hudVisible = ParseBool(value, true);
            } else if (key == "bWidgetAutoHide") {
                g_widgetAutoHide = ParseBool(value, false);
            } else if (key == "WidgetHoldSeconds") {
                ReadNumber(value, g_widgetHoldSeconds, 3, 60);
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
            } else if (key == "iFillMethod") {
                try {
                    g_fillMethod = static_cast<FillMethod>(std::clamp(clib_util::string::to_num<int>(value), 0, 2));
                    hasFillMethod = true;
                } catch (...) {
                }
            } else if (key == "bUseFillPower") {
                legacyFillPower = ParseBool(value, false);
            } else if (key == "bEnableDirtyWater") {
                g_enableDirtyWater = ParseBool(value, false);
            } else if (key == "fRiskLow") {
                ReadNumber(value, g_riskLow, 0.0f, 100.0f);
            } else if (key == "fRiskFoul") {
                ReadNumber(value, g_riskFoul, 0.0f, 100.0f);
            } else if (key == "fBottleQuench") {
                ReadNumber(value, g_bottleQuench, 15.0f, 75.0f);
            } else if (key == "bReuseBottles") {
                g_reuseBottles = ParseBool(value, true);
            } else if (key == "bEnablePerkGate") {
                g_enablePerkGate = ParseBool(value, false);
            } else if (key == "sPerkForms") {
                g_perkForms = value;
            } else if (key == "fPerkRateReduction") {
                ReadNumber(value, g_perkRateReduction, 15.0f, 75.0f);
            } else if (key == "bUseKynesEmbraceGlobal") {
                g_useKynesEmbraceGlobal = ParseBool(value, false);
            } else if (key == "bEnableLogging") {
                g_enableLogging = ParseBool(value, false);
            } else if (key.rfind("SurvivalWidget", 0) == 0) {
                static constexpr const char* suffixes[SURVIVAL_WIDGET_COUNT] = {"Cold", "Hunger", "Exhaustion"};
                for (int i = 0; i < SURVIVAL_WIDGET_COUNT; ++i) {
                    const std::string base = std::string("SurvivalWidget") + suffixes[i];
                    if (key == base + "Enabled") {
                        g_survivalWidgetEnabled[i] = ParseBool(value, false);
                    } else if (key == base + "X") {
                        ReadNumber(value, g_survivalWidgetX[i], 0, INT_MAX);
                    } else if (key == base + "Y") {
                        ReadNumber(value, g_survivalWidgetY[i], 0, INT_MAX);
                    } else if (key == base + "Scale") {
                        ReadNumber(value, g_survivalWidgetScale[i], 10, 150);
                    }
                }
            } else if (key == "Difficulty") {
                const auto upper = clib_util::string::toupper(value);
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

        // ini from before the fill method dropdown
        if (!hasFillMethod && legacyFillPower) {
            g_fillMethod = FillMethod::Power;
        }

        if (g_enablePerkGate && g_difficulty != Difficulty::Hard && g_difficulty != Difficulty::VeryHard) {
            ApplyDifficulty(Difficulty::Hard);
        }

        ParsePerkForms();

        spdlog::set_level(g_enableLogging ? spdlog::level::info : spdlog::level::off);

        logger::info("[Settings] Loaded. FillKey={} ToggleWidgetKey={} HudX={} HudY={} WidgetScale={} HUDVisible={} Difficulty={}", GetKeyName(g_fillKey), GetKeyName(g_toggleWidgetKey), g_hudX, g_hudY, g_widgetScale, g_hudVisible ? 1 : 0, DIFFICULTY_NAMES[static_cast<int>(g_difficulty)]);
    }

    void SaveToINI() {
        const auto path = GetINIPath();
        if (path.empty()) return;

        std::vector<std::pair<std::string, std::string>> values = {
            {"bEnableTears", g_enableTears ? "1" : "0"},
            {"bEnableTearsWithSM", g_enableTearsWithSM ? "1" : "0"},
            {"Difficulty", std::to_string(static_cast<int>(g_difficulty))},
            {"bPauseNeedsInJail", g_pauseNeedsInJail ? "1" : "0"},
            {"bDisableForVampire", g_disableForVampire ? "1" : "0"},
            {"bDeathByDehydration", g_deathByDehydration ? "1" : "0"},
            {"bReuseBottles", g_reuseBottles ? "1" : "0"},
            {"fBottleQuench", std::format("{}", g_bottleQuench)},
            {"bEnableDirtyWater", g_enableDirtyWater ? "1" : "0"},
            {"fRiskLow", std::format("{}", g_riskLow)},
            {"fRiskFoul", std::format("{}", g_riskFoul)},
            {"bEnablePerkGate", g_enablePerkGate ? "1" : "0"},
            {"sPerkForms", g_perkForms},
            {"fPerkRateReduction", std::format("{}", g_perkRateReduction)},
            {"bUseKynesEmbraceGlobal", g_useKynesEmbraceGlobal ? "1" : "0"},
            {"bHudVisible", g_hudVisible ? "1" : "0"},
            {"HudX", std::to_string(g_hudX)},
            {"HudY", std::to_string(g_hudY)},
            {"WidgetScale", std::to_string(g_widgetScale)},
            {"bWidgetAutoHide", g_widgetAutoHide ? "1" : "0"},
            {"WidgetHoldSeconds", std::to_string(g_widgetHoldSeconds)},
            {"FillKey", GetKeyName(g_fillKey)},
            {"ToggleWidgetKey", g_toggleWidgetKey == 0 ? "0" : GetKeyName(g_toggleWidgetKey)},
            {"iFillMethod", std::to_string(static_cast<int>(g_fillMethod))},
            {"bEnableLogging", g_enableLogging ? "1" : "0"},
        };

        {
            static constexpr const char* suffixes[SURVIVAL_WIDGET_COUNT] = {"Cold", "Hunger", "Exhaustion"};
            for (int i = 0; i < SURVIVAL_WIDGET_COUNT; ++i) {
                const std::string base = std::string("SurvivalWidget") + suffixes[i];
                values.emplace_back(base + "Enabled", g_survivalWidgetEnabled[i] ? "1" : "0");
                values.emplace_back(base + "X", std::to_string(g_survivalWidgetX[i]));
                values.emplace_back(base + "Y", std::to_string(g_survivalWidgetY[i]));
                values.emplace_back(base + "Scale", std::to_string(g_survivalWidgetScale[i]));
            }
        }

        std::vector<std::string> lines;
        bool hadFile = false;
        {
            std::ifstream in(path);
            if (in.is_open()) {
                hadFile = true;
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '') line.pop_back();
                    lines.push_back(line);
                }
            }
        }

        std::vector<bool> written(values.size(), false);

        for (auto& line : lines) {
            const size_t firstNonSpace = line.find_first_not_of(" 	");
            if (firstNonSpace == std::string::npos) continue;
            const char c = line[firstNonSpace];
            if (c == ';' || c == '#' || c == '[') continue;

            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(firstNonSpace, eq - firstNonSpace);
            const size_t keyEnd = key.find_last_not_of(" 	");
            if (keyEnd != std::string::npos) key = key.substr(0, keyEnd + 1);

            for (size_t i = 0; i < values.size(); ++i) {
                if (!written[i] && key == values[i].first) {
                    line = values[i].first + "=" + values[i].second;
                    written[i] = true;
                    break;
                }
            }
        }

        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) {
            logger::error("[Settings] Could not write INI to '{}'.", path);
            return;
        }

        if (!hadFile) {
            out << "[Tears of Kyne]" << "\n";
        }

        for (const auto& line : lines) {
            out << line << "\n";
        }

        for (size_t i = 0; i < values.size(); ++i) {
            if (!written[i]) {
                out << values[i].first << "=" << values[i].second << "\n";
            }
        }

        logger::info("[Settings] Wrote INI to '{}'.", path);
    }
}