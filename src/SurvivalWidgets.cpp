#include "SurvivalWidgets.h"

#include <algorithm>
#include <array>

#include "Manager.h"
#include "Menu.h"
#include "Settings.h"


// Readapted code from SMI and Bottle Widgets.
// This was a fuck ass to make so i hope u like this shit.

namespace {
    struct NeedDesc {
        const char* label;
        const char* menuName;
        const char* fileName;
        const char* needGlobal;
        std::array<const char*, 5> stageGlobals;
    };

    constexpr std::array<NeedDesc, Settings::SURVIVAL_WIDGET_COUNT> DESCS = {{
        {"Cold", "TearsColdWidgetMenu", "exported/widgets/TearsCold", "Survival_ColdNeedValue",
         {"Survival_ColdStage1Value", "Survival_ColdStage2Value", "Survival_ColdStage3Value",
          "Survival_ColdStage4Value", "Survival_ColdStage5Value"}},
        {"Hunger", "TearsHungerWidgetMenu", "exported/widgets/TearsHunger", "Survival_HungerNeedValue",
         {"Survival_HungerStage1Value", "Survival_HungerStage2Value", "Survival_HungerStage3Value",
          "Survival_HungerStage4Value", "Survival_HungerStage5Value"}},
        {"Exhaustion", "TearsExhaustionWidgetMenu", "exported/widgets/TearsExhaustion", "Survival_ExhaustionNeedValue",
         {"Survival_ExhaustionStage1Value", "Survival_ExhaustionStage2Value", "Survival_ExhaustionStage3Value",
          "Survival_ExhaustionStage4Value", "Survival_ExhaustionStage5Value"}},
    }};

    constexpr const char* VAR_ROOT_VISIBLE = "_root._visible";
    constexpr const char* VAR_VISIBLE = "_root.widget._visible";
    constexpr const char* VAR_ALPHA = "_root.widget._alpha";
    constexpr const char* VAR_X = "_root.widget._x";
    constexpr const char* VAR_Y = "_root.widget._y";
    constexpr const char* VAR_XSCALE = "_root.widget._xscale";
    constexpr const char* VAR_YSCALE = "_root.widget._yscale";
    constexpr const char* FN_SET_LEVEL = "_root.widget.setBathColorLevel";

    constexpr int kStageCount = 6;

    constexpr const char* SURVIVAL_TOGGLE_GLOBAL = "Survival_ModeToggle";

    struct NeedForms {
        RE::TESGlobal* need = nullptr;
        std::array<RE::TESGlobal*, 5> stages{};
        bool valid = false;
    };

    struct NeedState {
        std::atomic<int> lastStage{-1};
        std::atomic<bool> lastVisible{false};
        std::atomic<bool> showQueued{false};
        std::atomic<int> fadeGen{0};
        std::atomic<int> alpha{0};
        std::atomic<bool> peekActive{false};
        std::chrono::steady_clock::time_point peekStart{};
    };

    constexpr int kFinalStage = 5;

    std::array<NeedForms, Settings::SURVIVAL_WIDGET_COUNT> s_forms{};
    std::array<NeedState, Settings::SURVIVAL_WIDGET_COUNT> s_state{};
    RE::TESGlobal* s_survivalToggle = nullptr;
    std::atomic<bool> s_available{false};
    std::atomic<bool> s_registered{false};

    bool ValidIndex(int index) { return index >= 0 && index < Settings::SURVIVAL_WIDGET_COUNT; }

    bool SurvivalModeOn() { return s_survivalToggle && s_survivalToggle->value > 0.0f; }

    bool SystemActive() {
        if (!SurvivalModeOn()) return false;
        const auto* manager = WaterNeedManager::GetSingleton();
        return manager->IsSystemEnabled() && !manager->IsPausedForVampire();
    }

    template <int N>
    class NeedWidgetMenu : public RE::IMenu {
    public:
        NeedWidgetMenu() {
            depthPriority = 0;
            menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen, RE::UI_MENU_FLAGS::kAllowSaving,
                          RE::UI_MENU_FLAGS::kRequiresUpdate, RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu);
            if (RE::BSScaleformManager::GetSingleton()->LoadMovie(this, uiMovie, DESCS[N].fileName)) {
                logger::info("[SurvivalWidgets] {} movie loaded.", DESCS[N].label);
            } else {
                logger::error("[SurvivalWidgets] Could not load '{}.swf'.", DESCS[N].fileName);
            }
            s_state[N].showQueued.store(false);
        }

        static RE::IMenu* Creator() { return new NeedWidgetMenu<N>(); }
    };

    using CreatorFn = RE::IMenu* (*)();

    constexpr std::array<CreatorFn, Settings::SURVIVAL_WIDGET_COUNT> CREATORS = {
        &NeedWidgetMenu<0>::Creator, &NeedWidgetMenu<1>::Creator, &NeedWidgetMenu<2>::Creator};

    RE::GPtr<RE::GFxMovieView> GetMovie(int index) {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return nullptr;
        if (ui->IsMenuOpen("Loading Menu")) return nullptr;
        auto menu = ui->GetMenu(DESCS[index].menuName);
        if (!menu || !menu->uiMovie) return nullptr;
        return menu->uiMovie;
    }

    void SetNumber(RE::GFxMovieView* movie, const char* path, double value) {
        RE::GFxValue v;
        v.SetNumber(value);
        movie->SetVariable(path, v);
    }

    void SetBool(RE::GFxMovieView* movie, const char* path, bool value) {
        RE::GFxValue v;
        v.SetBoolean(value);
        movie->SetVariable(path, v);
    }

    void EnsureMenuShown(int index) {
        auto* ui = RE::UI::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue) return;
        if (ui->IsMenuOpen(DESCS[index].menuName)) {
            s_state[index].showQueued.store(false);
            return;
        }
        if (!ui->IsMenuOpen("HUD Menu")) return;
        if (s_state[index].showQueued.exchange(true)) return;
        queue->AddMessage(DESCS[index].menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }

    void HideMenu(int index) {
        auto& state = s_state[index];
        ++state.fadeGen;
        state.showQueued.store(false);
        state.lastVisible.store(false);
        state.lastStage.store(-1);
        state.alpha.store(0);
        state.peekActive.store(false);

        auto* ui = RE::UI::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue) return;
        if (!ui->IsMenuOpen(DESCS[index].menuName)) return;
        queue->AddMessage(DESCS[index].menuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
    }

    int ComputeStage(int index) {
        const auto& forms = s_forms[index];
        if (!forms.valid) return 0;
        const float need = forms.need->value;
        for (int stage = 0; stage < 5; ++stage) {
            if (need <= forms.stages[stage]->value) return stage;
        }
        return 5;
    }

    void ApplyAlpha(int index, int alpha) {
        auto movie = GetMovie(index);
        if (!movie) return;

        if (alpha <= 0) {
            SetBool(movie.get(), VAR_VISIBLE, false);
            return;
        }

        SetBool(movie.get(), VAR_ROOT_VISIBLE, true);
        SetBool(movie.get(), VAR_VISIBLE, true);
        SetNumber(movie.get(), VAR_ALPHA, static_cast<double>(alpha));
    }

    void StartFade(int index, int targetAlpha) {
        auto& state = s_state[index];
        if (state.alpha.load() == targetAlpha) return;

        const int gen = ++state.fadeGen;
        const int start = state.alpha.load();

        std::thread([index, gen, start, targetAlpha] {
            constexpr int steps = 12;
            for (int i = 1; i <= steps; ++i) {
                if (s_state[index].fadeGen.load() != gen) return;
                const int alpha = start + (targetAlpha - start) * i / steps;
                s_state[index].alpha.store(alpha);
                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([index, alpha] { ApplyAlpha(index, alpha); });
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }).detach();
    }

    void ApplyAutoHideVisibility(int index, bool baseShow, int stage) {
        auto& state = s_state[index];
        const bool desired = baseShow && (stage >= kFinalStage || state.peekActive.load());
        if (desired == state.lastVisible.load()) return;
        if (desired) state.peekStart = std::chrono::steady_clock::now();
        state.lastVisible.store(desired);
        StartFade(index, desired ? 100 : 0);
    }

    void ApplyWidget(int index, bool force) {
        auto& state = s_state[index];
        const bool baseShow =
            Settings::g_survivalWidgetEnabled[index] && SystemActive() && !TearsWidget::IsSuppressed();
        const int stage = baseShow ? ComputeStage(index) : -1;
        const int prevStage = state.lastStage.load();

        if (Settings::g_widgetAutoHide && baseShow && prevStage != -1 && stage != prevStage) {
            if (stage >= kFinalStage) {
                state.peekActive.store(false);
            } else {
                state.peekActive.store(true);
                state.peekStart = std::chrono::steady_clock::now();
            }
        }

        if (!force && !Settings::g_widgetAutoHide && baseShow == state.lastVisible.load() && stage == prevStage) {
            return;
        }

        auto movie = GetMovie(index);
        if (!movie) return;

        if (baseShow) {
            SetBool(movie.get(), VAR_ROOT_VISIBLE, true);

            if (force || stage != prevStage) {
                RE::GFxValue arg;
                arg.SetNumber(static_cast<double>(std::clamp(stage + 1, 1, kStageCount)));
                movie->Invoke(FN_SET_LEVEL, nullptr, &arg, 1);
            }

            if (force || !state.lastVisible.load()) {
                SetNumber(movie.get(), VAR_X, static_cast<double>(Settings::g_survivalWidgetX[index]));
                SetNumber(movie.get(), VAR_Y, static_cast<double>(Settings::g_survivalWidgetY[index]));
                SetNumber(movie.get(), VAR_XSCALE, static_cast<double>(Settings::g_survivalWidgetScale[index]));
                SetNumber(movie.get(), VAR_YSCALE, static_cast<double>(Settings::g_survivalWidgetScale[index]));
            }
        }

        state.lastStage.store(stage);

        if (Settings::g_widgetAutoHide) {
            if (force) {
                const bool desired = baseShow && (stage >= kFinalStage || state.peekActive.load());
                ++state.fadeGen;
                state.alpha.store(desired ? 100 : 0);
                state.lastVisible.store(desired);
                ApplyAlpha(index, desired ? 100 : 0);
            } else {
                ApplyAutoHideVisibility(index, baseShow, stage);
            }
        } else {
            ++state.fadeGen;
            state.alpha.store(baseShow ? 100 : 0);
            state.lastVisible.store(baseShow);
            SetBool(movie.get(), VAR_VISIBLE, baseShow);
            if (baseShow) SetNumber(movie.get(), VAR_ALPHA, 100.0);
        }
    }

    void UpdateAll(bool force) {
        if (!s_available.load()) return;

        const bool systemActive = SystemActive();

        for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
            if (!s_forms[i].valid) continue;

            if (!Settings::g_survivalWidgetEnabled[i] || !systemActive) {
                if (s_state[i].lastVisible.load() || s_state[i].showQueued.load()) {
                    HideMenu(i);
                }
                continue;
            }

            EnsureMenuShown(i);
            ApplyWidget(i, force);
        }
    }

    void ResolveForms() {
        s_survivalToggle = RE::TESForm::LookupByEditorID<RE::TESGlobal>(SURVIVAL_TOGGLE_GLOBAL);
        if (!s_survivalToggle) {
            logger::warn("[SurvivalWidgets] {} not found, widgets will stay hidden.", SURVIVAL_TOGGLE_GLOBAL);
        }

        bool anyValid = false;
        for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
            auto& forms = s_forms[i];
            forms.need = RE::TESForm::LookupByEditorID<RE::TESGlobal>(DESCS[i].needGlobal);

            bool ok = forms.need != nullptr;
            for (int s = 0; s < 5; ++s) {
                forms.stages[s] = RE::TESForm::LookupByEditorID<RE::TESGlobal>(DESCS[i].stageGlobals[s]);
                if (!forms.stages[s]) ok = false;
            }

            forms.valid = ok;
            anyValid = anyValid || ok;
            if (!ok) {
                logger::info("[SurvivalWidgets] {} globals not found, widget unavailable.", DESCS[i].label);
            }
        }
        s_available.store(anyValid);
    }
}

void SurvivalWidgets::Init() {
    ResolveForms();

    if (!s_available.load()) {
        logger::info("[SurvivalWidgets] Survival Mode Improved not detected. Widgets disabled.");
        return;
    }

    if (s_registered.exchange(true)) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::error("[SurvivalWidgets] UI singleton unavailable, widgets not registered.");
        return;
    }

    for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
        if (!s_forms[i].valid) continue;
        ui->Register(DESCS[i].menuName, CREATORS[i]);
    }

    ResetSession();
    logger::info("[SurvivalWidgets] Registered survival widget menus.");
}

void SurvivalWidgets::ResetSession() {
    for (auto& state : s_state) {
        ++state.fadeGen;
        state.lastStage.store(-1);
        state.lastVisible.store(false);
        state.showQueued.store(false);
        state.alpha.store(0);
        state.peekActive.store(false);
    }
}

void SurvivalWidgets::TickAutoHide() {
    if (!s_available.load() || !Settings::g_widgetAutoHide) return;

    for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
        if (!s_forms[i].valid || !Settings::g_survivalWidgetEnabled[i]) continue;

        auto& state = s_state[i];
        if (state.peekActive.load()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::steady_clock::now() - state.peekStart)
                                     .count();
            if (elapsed >= std::clamp(Settings::g_widgetHoldSeconds, 3, 60)) {
                state.peekActive.store(false);
            }
        }

        const bool baseShow = SystemActive() && !TearsWidget::IsSuppressed();
        const int stage = baseShow ? ComputeStage(i) : -1;

        if (stage != state.lastStage.load()) {
            ApplyWidget(i, false);
        } else {
            ApplyAutoHideVisibility(i, baseShow, stage);
        }
    }
}

void SurvivalWidgets::Notify() { UpdateAll(false); }

void SurvivalWidgets::Refresh() { UpdateAll(true); }

void SurvivalWidgets::NotifyMenuEvent(const char* menuName, bool opening) {
    if (!s_available.load() || !menuName) return;

    const std::string_view name(menuName);

    for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
        if (name != DESCS[i].menuName) continue;
        if (opening && s_forms[i].valid) {
            s_state[i].showQueued.store(false);
            ApplyWidget(i, true);
        }
        return;
    }

    if (name == "HUD Menu") {
        if (opening) {
            UpdateAll(true);
        } else {
            for (int i = 0; i < Settings::SURVIVAL_WIDGET_COUNT; ++i) {
                if (s_forms[i].valid) HideMenu(i);
            }
        }
        return;
    }

    if (name == "Loading Menu" && !opening) {
        for (auto& state : s_state) {
            state.showQueued.store(false);
        }
    }

    UpdateAll(false);
}

bool SurvivalWidgets::IsAvailable() { return s_available.load(); }

bool SurvivalWidgets::IsNeedAvailable(int index) { return ValidIndex(index) && s_forms[index].valid; }

const char* SurvivalWidgets::GetLabel(int index) { return ValidIndex(index) ? DESCS[index].label : ""; }

void SurvivalWidgets::SetPosition(int index, int x, int y) {
    if (!ValidIndex(index)) return;
    Settings::g_survivalWidgetX[index] = x;
    Settings::g_survivalWidgetY[index] = y;

    if (auto movie = GetMovie(index)) {
        SetNumber(movie.get(), VAR_X, static_cast<double>(x));
        SetNumber(movie.get(), VAR_Y, static_cast<double>(y));
    }
}

void SurvivalWidgets::SetScale(int index, int scalePercent) {
    if (!ValidIndex(index)) return;
    const int scale = std::clamp(scalePercent, 10, 150);
    Settings::g_survivalWidgetScale[index] = scale;

    if (auto movie = GetMovie(index)) {
        SetNumber(movie.get(), VAR_XSCALE, static_cast<double>(scale));
        SetNumber(movie.get(), VAR_YSCALE, static_cast<double>(scale));
    }
}
