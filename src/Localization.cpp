#include "Localization.h"

#include <map>

namespace Localization {
    namespace {
        std::map<std::string, std::string> g_strings;

        std::map<std::string, std::string> g_fallback = {
            {"$TOK_AlreadyQuenched", "I'm already Quenched."},
            {"$TOK_NeedWater", "I need to stand in water to fill my waterskin."},
            {"$TOK_ReceivedWaterskin", "I received a water skin."},
            {"$TOK_WaterskinEmpty", "My waterskin is empty."},
            {"$TOK_BottleEmpty", "This bottle is empty."},
            {"$TOK_WaterskinFilled", "My waterskins are full."},
            {"$TOK_NoFilledWaterskin", "I do not have a filled waterskin."},
            {"$TOK_CouldNotDrink", "I could not drink from my waterskin."},
            {"$TOK_SheatheFirst", "I should sheathe my weapon first."},
            {"$TOK_CannotFillCombat", "I cannot do that during combat."},
            {"$TOK_DeathThirst", "You succumbed to dehydration."},
            {"$TOK_StageQuenched", "I'm Quenched."},
            {"$TOK_StageSated", "I'm Sated."},
            {"$TOK_StageThirsty", "I'm Thirsty."},
            {"$TOK_StageParched", "I'm Parched."},
            {"$TOK_StageDehydrated", "I'm Dehydrated."},
        };
    }

    void Load() {
        g_strings.clear();

        SKSE::Translation::ParseTranslation("TearsOfKyne");
        for (const auto& [key, fallbackValue] : g_fallback) {
            std::string value;
            if (SKSE::Translation::Translate(key, value)) {
                g_strings[key] = value;
            }
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