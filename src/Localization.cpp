#include "Localization.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace Localization {
    namespace {
        std::map<std::string, std::string> g_strings;

        std::map<std::string, std::string> g_fallback = {
            {"$TOK_AlreadyQuenched", "I'm already Quenched."},
            {"$TOK_NeedWater", "I need to stand in water to fill my waterskin."},
            {"$TOK_ReceivedWaterskin", "I received a water skin."},
            {"$TOK_WaterskinEmpty", "My waterskin is empty."},
            {"$TOK_WaterskinFilled", "My waterskins are full."},
            {"$TOK_NoFilledWaterskin", "I do not have a filled waterskin."},
            {"$TOK_CouldNotDrink", "I could not drink from my waterskin."},
            {"$TOK_SheatheFirst", "I should sheathe my weapon first."},
            {"$TOK_CannotFillCombat", "I cannot do that during combat."},
            {"$TOK_StageQuenched", "I'm Quenched."},
            {"$TOK_StageSated", "I'm Sated."},
            {"$TOK_StageThirsty", "I'm Thirsty."},
            {"$TOK_StageParched", "I'm Parched."},
            {"$TOK_StageDehydrated", "I'm Dehydrated."},
        };

        std::string Utf16ToUtf8(const std::u16string& in) {
            std::string out;
            out.reserve(in.size());
            for (size_t i = 0; i < in.size(); ++i) {
                char32_t cp = in[i];
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in.size()) {
                    const char32_t lo = in[i + 1];
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        ++i;
                    }
                }
                if (cp < 0x80) {
                    out.push_back(static_cast<char>(cp));
                } else if (cp < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp < 0x10000) {
                    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
            }
            return out;
        }

        std::string Trim(const std::string& s) {
            const size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            const size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }

        void ParseLines(const std::string& text) {
            size_t start = 0;
            while (start <= text.size()) {
                size_t nl = text.find('\n', start);
                std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                start = (nl == std::string::npos) ? text.size() + 1 : nl + 1;

                const size_t tab = line.find('\t');
                if (tab == std::string::npos) continue;
                std::string key = Trim(line.substr(0, tab));
                std::string value = Trim(line.substr(tab + 1));
                if (key.empty() || key[0] != '$') continue;
                g_strings[key] = value;
            }
        }
    }

    void Load() {
        g_strings.clear();

        const std::filesystem::path file = "Data/Interface/Translations/TearsOfKyne_Translation.txt";

        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            return;
        }

        std::ifstream in(file, std::ios::binary);
        if (!in.is_open()) return;

        std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE) {
            std::u16string u16;
            for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
                u16.push_back(static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
                                                    (static_cast<unsigned char>(bytes[i + 1]) << 8)));
            }
            ParseLines(Utf16ToUtf8(u16));
        } else {
            std::string text(bytes.begin(), bytes.end());
            if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
                static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
                text = text.substr(3);
            }
            ParseLines(text);
        }

        logger::info("[Localization] Loaded {} strings.", g_strings.size());
    }

    const std::string& Get(const char* key) {
        if (const auto it = g_strings.find(key); it != g_strings.end() && !it->second.empty()) {
            return it->second;
        }
        if (const auto it = g_fallback.find(key); it != g_fallback.end()) {
            return it->second;
        }
        static const std::string missing = "";
        return missing;
    }
}