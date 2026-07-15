#include "UI.h"

#include <cstring>

#include "SKSEMenuFramework.h"

namespace ImGui = ImGuiMCP;

#include "Hotkeys.h"
#include "Manager.h"
#include "Menu.h"
#include "Settings.h"
#include "Utils.h"

namespace {
    constexpr ImGui::ImVec4 GOLD{0.95f, 0.82f, 0.48f, 1.00f};
    constexpr ImGui::ImVec4 GOLD_DIM{0.70f, 0.62f, 0.42f, 0.85f};
    constexpr ImGui::ImVec4 GOLD_FRAME{0.28f, 0.23f, 0.12f, 0.50f};
    constexpr ImGui::ImVec4 GOLD_HOVER{0.40f, 0.33f, 0.17f, 0.65f};
    constexpr ImGui::ImVec4 GOLD_ACTIVE{0.55f, 0.45f, 0.24f, 0.80f};
    constexpr ImGui::ImVec4 GOLD_GRAB{0.90f, 0.76f, 0.42f, 1.00f};
    constexpr ImGui::ImVec4 SEP_LINE{0.55f, 0.46f, 0.26f, 0.45f};

    std::uint32_t ImGuiKeyToScanCode(int key) {
        using namespace ImGui;
        switch (key) {
            case ImGuiKey_A:
                return 0x1E;
            case ImGuiKey_B:
                return 0x30;
            case ImGuiKey_C:
                return 0x2E;
            case ImGuiKey_D:
                return 0x20;
            case ImGuiKey_E:
                return 0x12;
            case ImGuiKey_F:
                return 0x21;
            case ImGuiKey_G:
                return 0x22;
            case ImGuiKey_H:
                return 0x23;
            case ImGuiKey_I:
                return 0x17;
            case ImGuiKey_J:
                return 0x24;
            case ImGuiKey_K:
                return 0x25;
            case ImGuiKey_L:
                return 0x26;
            case ImGuiKey_M:
                return 0x32;
            case ImGuiKey_N:
                return 0x31;
            case ImGuiKey_O:
                return 0x18;
            case ImGuiKey_P:
                return 0x19;
            case ImGuiKey_Q:
                return 0x10;
            case ImGuiKey_R:
                return 0x13;
            case ImGuiKey_S:
                return 0x1F;
            case ImGuiKey_T:
                return 0x14;
            case ImGuiKey_U:
                return 0x16;
            case ImGuiKey_V:
                return 0x2F;
            case ImGuiKey_W:
                return 0x11;
            case ImGuiKey_X:
                return 0x2D;
            case ImGuiKey_Y:
                return 0x15;
            case ImGuiKey_Z:
                return 0x2C;
            case ImGuiKey_0:
                return 0x0B;
            case ImGuiKey_1:
                return 0x02;
            case ImGuiKey_2:
                return 0x03;
            case ImGuiKey_3:
                return 0x04;
            case ImGuiKey_4:
                return 0x05;
            case ImGuiKey_5:
                return 0x06;
            case ImGuiKey_6:
                return 0x07;
            case ImGuiKey_7:
                return 0x08;
            case ImGuiKey_8:
                return 0x09;
            case ImGuiKey_9:
                return 0x0A;
            case ImGuiKey_F1:
                return 0x3B;
            case ImGuiKey_F2:
                return 0x3C;
            case ImGuiKey_F3:
                return 0x3D;
            case ImGuiKey_F4:
                return 0x3E;
            case ImGuiKey_F5:
                return 0x3F;
            case ImGuiKey_F6:
                return 0x40;
            case ImGuiKey_F7:
                return 0x41;
            case ImGuiKey_F8:
                return 0x42;
            case ImGuiKey_F9:
                return 0x43;
            case ImGuiKey_F10:
                return 0x44;
            case ImGuiKey_F11:
                return 0x57;
            case ImGuiKey_F12:
                return 0x58;
            case ImGuiKey_Space:
                return 0x39;
            case ImGuiKey_Enter:
                return 0x1C;
            case ImGuiKey_Tab:
                return 0x0F;
            case ImGuiKey_Minus:
                return 0x0C;
            case ImGuiKey_Equal:
                return 0x0D;
            case ImGuiKey_LeftBracket:
                return 0x1A;
            case ImGuiKey_RightBracket:
                return 0x1B;
            case ImGuiKey_Semicolon:
                return 0x27;
            case ImGuiKey_Apostrophe:
                return 0x28;
            case ImGuiKey_GraveAccent:
                return 0x29;
            case ImGuiKey_Backslash:
                return 0x2B;
            case ImGuiKey_Comma:
                return 0x33;
            case ImGuiKey_Period:
                return 0x34;
            case ImGuiKey_Slash:
                return 0x35;
            case ImGuiKey_Keypad0:
                return 0x52;
            case ImGuiKey_Keypad1:
                return 0x4F;
            case ImGuiKey_Keypad2:
                return 0x50;
            case ImGuiKey_Keypad3:
                return 0x51;
            case ImGuiKey_Keypad4:
                return 0x4B;
            case ImGuiKey_Keypad5:
                return 0x4C;
            case ImGuiKey_Keypad6:
                return 0x4D;
            case ImGuiKey_Keypad7:
                return 0x47;
            case ImGuiKey_Keypad8:
                return 0x48;
            case ImGuiKey_Keypad9:
                return 0x49;
            case ImGuiKey_Home:
                return 0xC7;
            case ImGuiKey_End:
                return 0xCF;
            case ImGuiKey_Insert:
                return 0xD2;
            case ImGuiKey_Delete:
                return 0xD3;
            case ImGuiKey_PageUp:
                return 0xC9;
            case ImGuiKey_PageDown:
                return 0xD1;
            case ImGuiKey_UpArrow:
                return 0xC8;
            case ImGuiKey_DownArrow:
                return 0xD0;
            case ImGuiKey_LeftArrow:
                return 0xCB;
            case ImGuiKey_RightArrow:
                return 0xCD;
            default:
                return 0u;
        }
    }

    void PollKeyCapture() {
        if (!TearsWidget::IsAwaitingBinding()) return;

        if (ImGui::IsKeyPressed(ImGui::ImGuiKey_Escape, false)) {
            TearsWidget::CancelBinding();
            return;
        }
        for (int k = ImGui::ImGuiKey_NamedKey_BEGIN; k < ImGui::ImGuiKey_NamedKey_END; ++k) {
            if (!ImGui::IsKeyPressed(static_cast<ImGui::ImGuiKey>(k), false)) continue;
            const std::uint32_t scan = ImGuiKeyToScanCode(k);
            if (scan != 0) {
                TearsWidget::CaptureBindingKey(scan);
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
        bool enableTears = Settings::g_enableTears;
        RowLabel("Enable Tears of Kyne");
        if (ImGui::Checkbox("##enable", &enableTears)) {
            Settings::g_enableTears = enableTears;
            Settings::SaveToINI();
            if (!Settings::g_enableTearsWithSM) {
                WaterskinUtils::SetSystemEnabled(enableTears);
                TearsWidget::Refresh();
            }
        }

        bool syncSM = Settings::g_enableTearsWithSM;
        RowLabel("Sync with Survival Mode");
        if (ImGui::Checkbox("##syncsm", &syncSM)) {
            Settings::g_enableTearsWithSM = syncSM;
            Settings::SaveToINI();
        }
        ImGui::EndTable();
    }

    PopGoldTheme();
}

void __stdcall UI::RenderGameplay() {
    PushGoldTheme();

    Heading("Gameplay");
    if (BeginSettingsTable("gameplay_tbl")) {
        static const char* difficultyNames[] = {"Easy", "Medium", "Hard", "Very Hard"};
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
        } else {
            if (ImGui::Combo("##difficulty", &difficulty, difficultyNames, 4)) {
                Settings::ApplyDifficulty(static_cast<Settings::Difficulty>(std::clamp(difficulty, 0, 3)));
                Settings::SaveToINI();
                TearsWidget::Refresh();
            }
        }

        bool pauseJail = Settings::g_pauseNeedsInJail;
        RowLabel("Pause Needs in Jail");
        if (ImGui::Checkbox("##jail", &pauseJail)) {
            Settings::g_pauseNeedsInJail = pauseJail;
            Settings::SaveToINI();
        }

        bool vampire = Settings::g_disableForVampire;
        RowLabel("No Thirst as Vampire");
        if (ImGui::Checkbox("##vampire", &vampire)) {
            Settings::g_disableForVampire = vampire;
            Settings::SaveToINI();
            TearsWidget::Refresh();
        }

        bool deathToggle = Settings::g_deathByDehydration;
        RowLabel("Death by Dehydration");
        if (ImGui::Checkbox("##deathdehydration", &deathToggle)) {
            Settings::g_deathByDehydration = deathToggle;
            Settings::SaveToINI();
        }

        bool reuseBottles = Settings::g_reuseBottles;
        RowLabel("Reusable Bottles");
        if (ImGui::Checkbox("##reusebottles", &reuseBottles)) {
            Settings::g_reuseBottles = reuseBottles;
            Settings::SaveToINI();
        }

        if (Settings::g_reuseBottles) {
            float bottleQuench = Settings::g_bottleQuench;
            RowLabel("Bottle Quench");
            if (ImGui::SliderFloat("##bottlequench", &bottleQuench, 15.0f, 75.0f, "%.0f%%")) {
                Settings::g_bottleQuench = std::clamp(bottleQuench, 15.0f, 75.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Settings::SaveToINI();
            }
        }

        ImGui::EndTable();
    }
    if (Settings::g_deathByDehydration) {
        ImGui::TextColored(GOLD_DIM, "You will die if you go 5 days without drinking.");
    }

    Heading("Perk Gate");
    if (BeginSettingsTable("perk_tbl")) {
        bool perkGate = Settings::g_enablePerkGate;
        RowLabel("Enable Perk Gate");
        if (ImGui::Checkbox("##perkgate", &perkGate)) {
            Settings::g_enablePerkGate = perkGate;
            if (perkGate && Settings::g_difficulty != Settings::Difficulty::Hard &&
                Settings::g_difficulty != Settings::Difficulty::VeryHard) {
                Settings::ApplyDifficulty(Settings::Difficulty::Hard);
            }
            Settings::SaveToINI();
            TearsWidget::Refresh();
        }

        float reduction = Settings::g_perkRateReduction;
        RowLabel("Rate Reduction");
        if (ImGui::SliderFloat("##perkrate", &reduction, 15.0f, 75.0f, "%.0f%%")) {
            Settings::g_perkRateReduction = std::clamp(reduction, 15.0f, 75.0f);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::SaveToINI();
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

    const int parsed = Settings::GetParsedPerkCount();
    const int requested = Settings::GetRequestedPerkCount();
    ImGui::TextColored(GOLD_DIM, "%d / %d Perk Parsed", parsed, requested);

    Heading("Dirty Water");
    if (BeginSettingsTable("dirty_tbl")) {
        bool enableDirty = Settings::g_enableDirtyWater;
        RowLabel("Enable Dirty Water");
        if (ImGui::Checkbox("##enabledirty", &enableDirty)) {
            Settings::g_enableDirtyWater = enableDirty;
            Settings::SaveToINI();
            WaterskinUtils::SyncDirtyWater();
        }

        if (Settings::g_enableDirtyWater) {
            float riskLow = Settings::g_riskLow;
            RowLabel("River Sickness Chance");
            if (ImGui::SliderFloat("##risklow", &riskLow, 0.0f, 100.0f, "%.0f%%")) {
                Settings::g_riskLow = std::clamp(riskLow, 0.0f, 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Settings::SaveToINI();
            }

            float riskFoul = Settings::g_riskFoul;
            RowLabel("Swamp Sickness Chance");
            if (ImGui::SliderFloat("##riskfoul", &riskFoul, 0.0f, 100.0f, "%.0f%%")) {
                Settings::g_riskFoul = std::clamp(riskFoul, 0.0f, 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Settings::SaveToINI();
            }
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
        bool hudVisible = Settings::g_hudVisible;
        RowLabel("Show Widget");
        if (ImGui::Checkbox("##showwidget", &hudVisible)) {
            Settings::g_hudVisible = hudVisible;
            Settings::SaveToINI();
            TearsWidget::Refresh();
        }

        int scale = Settings::g_widgetScale;
        RowLabel("Scale");
        if (ImGui::SliderInt("##scale", &scale, 10, 150, "%d%%")) {
            TearsWidget::SetScale(scale);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::SaveToINI();
        }

        bool autoHide = Settings::g_widgetAutoHide;
        RowLabel("Auto-Hide Widget");
        if (ImGui::Checkbox("##autohide", &autoHide)) {
            Settings::g_widgetAutoHide = autoHide;
            Settings::SaveToINI();
            TearsWidget::Refresh();
        }

        if (Settings::g_widgetAutoHide) {
            int hold = Settings::g_widgetHoldSeconds;
            RowLabel("Hold Time");
            if (ImGui::SliderInt("##holdsec", &hold, 3, 60, "%d s")) {
                Settings::g_widgetHoldSeconds = std::clamp(hold, 3, 60);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Settings::SaveToINI();
            }
        }
        ImGui::EndTable();
    }
    if (Settings::g_widgetAutoHide) {
        ImGui::TextColored(GOLD_DIM, "Widget fades in when your thirst changes, then hides. Stays at full thirst.");
    }

    Heading("Position");
    ImGui::TextColored(GOLD_DIM, "Ctrl+click a slider to type a value.");
    if (BeginSettingsTable("pos_tbl")) {
        int hudX = Settings::g_hudX;
        RowLabel("X");
        if (ImGui::SliderInt("##posx", &hudX, 0, 1280)) {
            TearsWidget::SetPosition(hudX, Settings::g_hudY);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::SaveToINI();
        }

        int hudY = Settings::g_hudY;
        RowLabel("Y");
        if (ImGui::SliderInt("##posy", &hudY, 0, 720)) {
            TearsWidget::SetPosition(Settings::g_hudX, hudY);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::SaveToINI();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Reset Position")) {
        TearsWidget::SetPosition(Settings::DEFAULT_HUD_X, Settings::DEFAULT_HUD_Y);
        Settings::SaveToINI();
    }

    Heading("Keybinds");

    bool useFillPower = Settings::g_useFillPower;
    if (ImGui::Checkbox("Fill via Power", &useFillPower)) {
        Settings::g_useFillPower = useFillPower;
        if (useFillPower) TearsWidget::CancelBinding();
        Settings::SaveToINI();
        Hotkeys::RefreshBindings();
        WaterskinUtils::SyncFillPower();
    }
    ImGui::TextColored(GOLD_DIM, "Adds a Castable Power and Disables the Hotkey");

    if (BeginSettingsTable("keybind_tbl")) {
        const bool awaitingFill = TearsWidget::IsAwaitingBinding() && TearsWidget::GetPendingBinding() == "fill";
        RowLabel("Drink / Fill at Water");
        if (Settings::g_useFillPower) {
            ImGui::TextUnformatted("Using Power");
        } else if (awaitingFill) {
            ImGui::TextUnformatted("press a key...");
        } else {
            ImGui::TextUnformatted(Settings::GetKeyName(Settings::g_fillKey).c_str());
        }
        if (!Settings::g_useFillPower) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (awaitingFill) {
                if (ImGui::Button("Cancel##fill")) TearsWidget::CancelBinding();
            } else {
                if (ImGui::Button("Rebind##fill")) TearsWidget::BeginBinding("fill");
            }
        }

        const bool awaitingToggle =
            TearsWidget::IsAwaitingBinding() && TearsWidget::GetPendingBinding() == "togglewidget";
        RowLabel("Toggle Widget");
        if (awaitingToggle) {
            ImGui::TextUnformatted("press a key...");
        } else {
            ImGui::TextUnformatted(Settings::g_toggleWidgetKey == 0
                                       ? "Unbound"
                                       : Settings::GetKeyName(Settings::g_toggleWidgetKey).c_str());
        }
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
                Hotkeys::RefreshBindings();
            }
        }
        ImGui::EndTable();
    }

    PopGoldTheme();
}