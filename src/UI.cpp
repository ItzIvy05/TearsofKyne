#include "UI.h"

#include <cstring>

#include <SKSEMCP/SKSEMenuFramework.hpp>

namespace ImGui = ImGuiMCP;

#include "Menu.h"
#include "Settings.h"
#include "SurvivalWidgets.h"
#include "Utils.h"

namespace {
    constexpr ImGui::ImVec4 GOLD{0.95f, 0.82f, 0.48f, 1.00f};
    constexpr ImGui::ImVec4 GOLD_DIM{0.70f, 0.62f, 0.42f, 0.85f};
    constexpr ImGui::ImVec4 GOLD_FRAME{0.28f, 0.23f, 0.12f, 0.50f};
    constexpr ImGui::ImVec4 GOLD_HOVER{0.40f, 0.33f, 0.17f, 0.65f};
    constexpr ImGui::ImVec4 GOLD_ACTIVE{0.55f, 0.45f, 0.24f, 0.80f};
    constexpr ImGui::ImVec4 GOLD_GRAB{0.90f, 0.76f, 0.42f, 1.00f};
    constexpr ImGui::ImVec4 SEP_LINE{0.55f, 0.46f, 0.26f, 0.45f};

    void PollKeyCapture() {
        if (!TearsWidget::IsAwaitingBinding()) return;

        auto* keyboard = RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        if (!keyboard) return;

        // same check as IsPressed, which drags in the device's unresolved vfuncs
        const auto& keys = keyboard->GetRuntimeData().curState;
        for (std::uint32_t key = 1; key < std::size(keys); ++key) {
            if (keys[key] & 0x80) {
                TearsWidget::CaptureBindingKey(key);
                return;
            }
        }
    }

    void PushGoldTheme() {
        ImGui::PushStyleColor(ImGui::ImGuiCol_CheckMark, GOLD);
        ImGui::PushStyleColor(ImGui::ImGuiCol_SliderGrab, GOLD_GRAB);
        ImGui::PushStyleColor(ImGui::ImGuiCol_SliderGrabActive, GOLD);
        ImGui::PushStyleColor(ImGui::ImGuiCol_FrameBg, GOLD_FRAME);
        ImGui::PushStyleColor(ImGui::ImGuiCol_FrameBgHovered, GOLD_HOVER);
        ImGui::PushStyleColor(ImGui::ImGuiCol_FrameBgActive, GOLD_ACTIVE);
        ImGui::PushStyleColor(ImGui::ImGuiCol_Button, GOLD_FRAME);
        ImGui::PushStyleColor(ImGui::ImGuiCol_ButtonHovered, GOLD_HOVER);
        ImGui::PushStyleColor(ImGui::ImGuiCol_ButtonActive, GOLD_ACTIVE);
        ImGui::PushStyleColor(ImGui::ImGuiCol_Header, GOLD_FRAME);
        ImGui::PushStyleColor(ImGui::ImGuiCol_HeaderHovered, GOLD_HOVER);
        ImGui::PushStyleColor(ImGui::ImGuiCol_HeaderActive, GOLD_ACTIVE);
        ImGui::PushStyleColor(ImGui::ImGuiCol_Separator, SEP_LINE);
    }

    void PopGoldTheme() { ImGui::PopStyleColor(13); }

    void Heading(const char* label) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGui::ImGuiCol_Text, GOLD);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
    }

    bool BeginSettingsTable(const char* id) {
        if (ImGui::BeginTable(id, 2, ImGui::ImGuiTableFlags_SizingStretchProp | ImGui::ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("label", ImGui::ImGuiTableColumnFlags_WidthStretch, 0.41f);
            ImGui::TableSetupColumn("control", ImGui::ImGuiTableColumnFlags_WidthStretch, 0.59f);
            return true;
        }
        return false;
    }

    void RowLabel(const char* text) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
    }

    bool CheckboxRow(const char* label, const char* id, bool& value) {
        RowLabel(label);
        if (!ImGui::Checkbox(id, &value)) return false;
        Settings::SaveToINI();
        return true;
    }

    void PercentSliderRow(const char* label, const char* id, float& value, float min, float max) {
        RowLabel(label);
        if (ImGui::SliderFloat(id, &value, min, max, "%.0f%%")) value = std::clamp(value, min, max);
        if (ImGui::IsItemDeactivatedAfterEdit()) Settings::SaveToINI();
    }

    bool IntSliderRow(const char* label, const char* id, int& value, int min, int max, const char* format) {
        RowLabel(label);
        const bool changed = ImGui::SliderInt(id, &value, min, max, format);
        if (ImGui::IsItemDeactivatedAfterEdit()) Settings::SaveToINI();
        return changed;
    }

    // the HUD stage is wider than 1280x720 on ultrawide, so its left edge goes negative
    RE::GRectF GetStageRect() {
        auto movie = RE::UI::GetSingleton()->GetMovieView(RE::HUDMenu::MENU_NAME);
        return movie ? movie->GetVisibleFrameRect() : RE::GRectF{0.0f, 0.0f, 1280.0f, 720.0f};
    }
}

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        logger::warn("[UI] SKSE Menu Framework not installed.");
        return;
    }
    SKSEMenuFramework::SetSection("Tears of Kyne");
    SKSEMenuFramework::AddSectionItem("System", UI::RenderSystem);
    SKSEMenuFramework::AddSectionItem("Gameplay", UI::RenderGameplay);
    SKSEMenuFramework::AddSectionItem("UI & Keybind", UI::RenderUIKeybind);
    logger::info("[UI] Registered settings pages.");
}

void __stdcall UI::RenderSystem() {
    PushGoldTheme();

    Heading("System");
    if (BeginSettingsTable("system_tbl")) {
        if (CheckboxRow("Enable Tears of Kyne", "##enable", Settings::g_enableTears) && !Settings::g_enableTearsWithSM) {
            WaterskinUtils::SetSystemEnabled(Settings::g_enableTears);
            TearsWidget::Refresh();
        }
        CheckboxRow("Sync with Survival Mode", "##syncsm", Settings::g_enableTearsWithSM);
        ImGui::EndTable();
    }

    PopGoldTheme();
}

void __stdcall UI::RenderGameplay() {
    PushGoldTheme();

    Heading("Gameplay");
    if (BeginSettingsTable("gameplay_tbl")) {
        int difficulty = static_cast<int>(Settings::g_difficulty);
        RowLabel("Difficulty");
        if (Settings::g_enablePerkGate) {
            static const char* lockedNames[] = {"Hard", "Very Hard"};
            int lockedSel = (difficulty == 3) ? 1 : 0;
            if (ImGui::Combo("##difficulty", &lockedSel, lockedNames, 2)) {
                Settings::ApplyDifficulty(lockedSel == 1 ? Settings::Difficulty::VeryHard : Settings::Difficulty::Hard);
                Settings::SaveToINI();
                TearsWidget::Refresh();
            }
            ImGui::TextColored(GOLD_DIM, "Locked to Hard / Very Hard by Perk Gate.");
        } else if (ImGui::Combo("##difficulty", &difficulty, Settings::DIFFICULTY_NAMES, 4)) {
            Settings::ApplyDifficulty(static_cast<Settings::Difficulty>(std::clamp(difficulty, 0, 3)));
            Settings::SaveToINI();
            TearsWidget::Refresh();
        }

        CheckboxRow("Pause Needs in Jail", "##jail", Settings::g_pauseNeedsInJail);
        if (CheckboxRow("No Thirst as Vampire", "##vampire", Settings::g_disableForVampire)) {
            TearsWidget::Refresh();
        }
        CheckboxRow("Death by Dehydration", "##deathdehydration", Settings::g_deathByDehydration);
        if (Settings::g_deathByDehydration) {
            ImGui::TextColored(GOLD_DIM, "You will die if you go 5 days without drinking.");
        }
        CheckboxRow("Reusable Bottles", "##reusebottles", Settings::g_reuseBottles);
        if (Settings::g_reuseBottles) {
            PercentSliderRow("Bottle Quench", "##bottlequench", Settings::g_bottleQuench, 15.0f, 75.0f);
        }
        ImGui::EndTable();
    }

    Heading("Perk Gate");
    if (BeginSettingsTable("perk_tbl")) {
        if (CheckboxRow("Enable Perk Gate", "##perkgate", Settings::g_enablePerkGate)) {
            if (Settings::g_enablePerkGate && Settings::g_difficulty != Settings::Difficulty::Hard && Settings::g_difficulty != Settings::Difficulty::VeryHard) {
                Settings::ApplyDifficulty(Settings::Difficulty::Hard);
                Settings::SaveToINI();
            }
            TearsWidget::Refresh();
        }
        CheckboxRow("Use Kyne's Embrace Global", "##perkglobal", Settings::g_useKynesEmbraceGlobal);
        if (!Settings::g_useKynesEmbraceGlobal) {
            PercentSliderRow("Rate Reduction", "##perkrate", Settings::g_perkRateReduction, 15.0f, 75.0f);
        }
        ImGui::EndTable();
    }

    static char perkBuf[1024] = {};
    static bool perkBufInit = false;
    if (!perkBufInit) {
        std::strncpy(perkBuf, Settings::g_perkForms.c_str(), sizeof(perkBuf) - 1);
        perkBufInit = true;
    }
    ImGui::TextColored(GOLD_DIM, "Perk  (Plugin.esm|FormID)");
    if (ImGui::InputText("##perklist", perkBuf, sizeof(perkBuf))) {
        Settings::g_perkForms = perkBuf;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Settings::SaveToINI();
        Settings::g_perkFormsDirty.store(true);
    }

    ImGui::TextColored(GOLD_DIM, "%d / %d Perk Parsed", Settings::GetParsedPerkCount(), Settings::GetRequestedPerkCount());

    Heading("Dirty Water");
    if (BeginSettingsTable("dirty_tbl")) {
        if (CheckboxRow("Enable Dirty Water", "##enabledirty", Settings::g_enableDirtyWater)) {
            WaterskinUtils::SyncDirtyWater();
        }
        if (Settings::g_enableDirtyWater) {
            PercentSliderRow("River Sickness Chance", "##risklow", Settings::g_riskLow, 0.0f, 100.0f);
            PercentSliderRow("Swamp Sickness Chance", "##riskfoul", Settings::g_riskFoul, 0.0f, 100.0f);
        }
        ImGui::EndTable();
    }
    ImGui::TextColored(GOLD_DIM, "Water from the wild must be boiled at a cooking pot before it is safe.");

    PopGoldTheme();
}

void __stdcall UI::RenderUIKeybind() {
    PushGoldTheme();
    PollKeyCapture();

    Heading("Widget");
    if (BeginSettingsTable("widget_tbl")) {
        if (CheckboxRow("Show Widget", "##showwidget", Settings::g_hudVisible)) {
            TearsWidget::Refresh();
            SurvivalWidgets::Refresh();
        }
        int scale = Settings::g_widgetScale;
        if (IntSliderRow("Scale", "##scale", scale, 10, 150, "%d%%")) TearsWidget::SetScale(scale);
        if (CheckboxRow("Auto-Hide Widget", "##autohide", Settings::g_widgetAutoHide)) {
            TearsWidget::Refresh();
            SurvivalWidgets::Refresh();
        }
        if (Settings::g_widgetAutoHide) {
            if (IntSliderRow("Hold Time", "##holdsec", Settings::g_widgetHoldSeconds, 3, 60, "%d s")) Settings::g_widgetHoldSeconds = std::clamp(Settings::g_widgetHoldSeconds, 3, 60);
        }
        ImGui::EndTable();
    }
    if (Settings::g_widgetAutoHide) {
        ImGui::TextColored(GOLD_DIM, "Widget fades in when your thirst changes, then hides. Stays at full thirst.");
    }

    Heading("Position");
    ImGui::TextColored(GOLD_DIM, "Ctrl+click a slider to type a value.");
    const auto stage = GetStageRect();
    if (BeginSettingsTable("pos_tbl")) {
        int x = Settings::g_hudX;
        if (IntSliderRow("X", "##posx", x, static_cast<int>(stage.left), static_cast<int>(stage.right), "%d")) TearsWidget::SetPosition(x, Settings::g_hudY);
        int y = Settings::g_hudY;
        if (IntSliderRow("Y", "##posy", y, static_cast<int>(stage.top), static_cast<int>(stage.bottom), "%d")) TearsWidget::SetPosition(Settings::g_hudX, y);
        ImGui::EndTable();
    }
    if (ImGui::Button("Reset Position")) {
        TearsWidget::SetPosition(Settings::DEFAULT_HUD_X, Settings::DEFAULT_HUD_Y);
        Settings::SaveToINI();
    }

    if (SurvivalWidgets::IsAvailable()) {
        Heading("Survival Mode Widgets");
        ImGui::TextColored(GOLD_DIM, "Detected Survival Mode Improved. These use its need stages.");

        for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
            if (!SurvivalWidgets::IsNeedAvailable(i)) continue;

            ImGui::PushID(i);
            if (BeginSettingsTable("survival_tbl")) {
                if (CheckboxRow(SurvivalWidgets::GetLabel(i), "##enabled", Settings::g_survivalWidgetEnabled[i])) {
                    SurvivalWidgets::Refresh();
                }
                if (Settings::g_survivalWidgetEnabled[i]) {
                    int scale = Settings::g_survivalWidgetScale[i];
                    if (IntSliderRow("Scale", "##sscale", scale, 10, 150, "%d%%")) SurvivalWidgets::SetScale(i, scale);
                    int x = Settings::g_survivalWidgetX[i];
                    if (IntSliderRow("X", "##sposx", x, static_cast<int>(stage.left), static_cast<int>(stage.right), "%d")) SurvivalWidgets::SetPosition(i, x, Settings::g_survivalWidgetY[i]);
                    int y = Settings::g_survivalWidgetY[i];
                    if (IntSliderRow("Y", "##sposy", y, static_cast<int>(stage.top), static_cast<int>(stage.bottom), "%d")) SurvivalWidgets::SetPosition(i, Settings::g_survivalWidgetX[i], y);
                }
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
    }

    Heading("Keybinds");

    if (BeginSettingsTable("keybind_tbl")) {
        static constexpr const char* fillMethodHints[] = {"Press the key at water to fill.", "Cast the power at water to fill.", "Use an empty waterskin or bottle at water to fill."};
        int fillMethod = static_cast<int>(Settings::g_fillMethod);
        RowLabel("Fill Method");
        if (ImGui::Combo("##fillmethod", &fillMethod, Settings::FILL_METHOD_NAMES, 3)) {
            Settings::g_fillMethod = static_cast<Settings::FillMethod>(fillMethod);
            if (Settings::g_fillMethod != Settings::FillMethod::Hotkey) TearsWidget::CancelBinding();
            Settings::SaveToINI();
            WaterskinUtils::SyncFillPower();
        }
        ImGui::TextColored(GOLD_DIM, "%s", fillMethodHints[static_cast<int>(Settings::g_fillMethod)]);

        const bool useHotkey = Settings::g_fillMethod == Settings::FillMethod::Hotkey;
        const bool awaitingFill = TearsWidget::IsAwaitingBinding() && TearsWidget::GetPendingBinding() == "fill";
        RowLabel("Drink / Fill at Water");
        if (!useHotkey) {
            ImGui::TextUnformatted(Settings::g_fillMethod == Settings::FillMethod::Power ? "Using Power" : "Using Waterskin");
        } else {
            ImGui::TextUnformatted(awaitingFill ? "press a key..." : Settings::GetKeyName(Settings::g_fillKey).c_str());
        }
        if (useHotkey) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (awaitingFill) {
                if (ImGui::Button("Cancel##fill")) TearsWidget::CancelBinding();
            } else {
                if (ImGui::Button("Rebind##fill")) TearsWidget::BeginBinding("fill");
            }
        }

        const bool awaitingToggle = TearsWidget::IsAwaitingBinding() && TearsWidget::GetPendingBinding() == "togglewidget";
        RowLabel("Toggle Widget");
        ImGui::TextUnformatted(awaitingToggle ? "press a key..." : Settings::GetKeyName(Settings::g_toggleWidgetKey).c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (awaitingToggle) {
            if (ImGui::Button("Cancel##toggle")) TearsWidget::CancelBinding();
        } else {
            if (ImGui::Button("Rebind##toggle")) TearsWidget::BeginBinding("togglewidget");
            ImGui::SameLine();
            if (ImGui::Button("Unbind##toggle")) {
                Settings::g_toggleWidgetKey = 0;
                Settings::SaveToINI();
            }
        }
        ImGui::EndTable();
    }

    PopGoldTheme();
}