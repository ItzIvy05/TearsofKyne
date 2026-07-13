#include "Menu.h"

#include <algorithm>
#include <array>

#include "Hotkeys.h"
#include "Manager.h"
#include "Settings.h"
#include "Utils.h"

namespace {
    std::atomic<bool> s_menuSuppressed{true};
    std::atomic<bool> s_sessionPending{false};
    std::atomic<bool> s_inGameSession{false};
    std::atomic<bool> s_requireRaceMenuExit{false};
    std::atomic<bool> s_raceMenuSeen{false};
    std::string s_pendingBinding;

    constexpr const char* WIDGET_ROOT = "_root.widget";

    int s_lastStage = -1;
    std::atomic<bool> s_peekActive{false};
    std::chrono::steady_clock::time_point s_peekStart{};
    std::atomic<int> s_fadeGen{0};
    std::atomic<int> s_currentAlpha{0};
    std::atomic<bool> s_autoHideVisible{false};

    constexpr int s_finalStage = 4;

    void ResetAutoHideState() {
        ++s_fadeGen;
        s_currentAlpha.store(0);
        s_autoHideVisible.store(false);
        s_peekActive.store(false);
        s_lastStage = -1;
    }

    std::atomic<bool> s_showQueued{false};

    class TearsWidgetMenu : public RE::IMenu {
    public:
        static constexpr const char* MENU_NAME = "TearsWidgetMenu";
        static constexpr const char* FILE_NAME = "exported/widgets/TearsStatus";

        TearsWidgetMenu() {
            depthPriority = 0;
            menuFlags.set(RE::UI_MENU_FLAGS::kAlwaysOpen, RE::UI_MENU_FLAGS::kAllowSaving,
                          RE::UI_MENU_FLAGS::kRequiresUpdate, RE::UI_MENU_FLAGS::kAdvancesUnderPauseMenu);
            if (RE::BSScaleformManager::GetSingleton()->LoadMovie(this, uiMovie, FILE_NAME)) {
                logger::info("[TearsWidget] Widget movie loaded.");
            } else {
                logger::error("[TearsWidget] Could not load '{}.swf' into widget menu.", FILE_NAME);
            }
            s_showQueued.store(false);
        }

        static RE::IMenu* Creator() { return new TearsWidgetMenu(); }
    };

    void EnsureMenuShown() {
        auto* ui = RE::UI::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue) return;
        if (ui->IsMenuOpen(TearsWidgetMenu::MENU_NAME)) {
            s_showQueued.store(false);
            return;
        }
        if (!ui->IsMenuOpen("HUD Menu")) return;
        if (s_showQueued.exchange(true)) return;
        queue->AddMessage(TearsWidgetMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
        logger::info("[TearsWidget] Widget menu show queued.");
    }

    void HideMenu() {
        s_showQueued.store(false);
        auto* ui = RE::UI::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue) return;
        if (!ui->IsMenuOpen(TearsWidgetMenu::MENU_NAME)) return;
        queue->AddMessage(TearsWidgetMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
    }

    void ForceRootVisible(RE::GFxMovieView* movie) {
        if (!movie) return;
        RE::GFxValue rootVisible;
        rootVisible.SetBoolean(true);
        movie->SetVariable("_root._visible", rootVisible);
    }

    bool ShouldSuppressForMenus() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || !player->GetParentCell()) return true;

        auto* ui = RE::UI::GetSingleton();
        if (!ui) return true;

        static constexpr std::array<std::string_view, 22> blockingMenus = {
            "BarterMenu",    "Book Menu",     "Console",       "ContainerMenu",  "Crafting Menu", "Dialogue Menu",
            "FavoritesMenu", "GiftMenu",      "InventoryMenu", "Journal Menu",   "Loading Menu",  "Lockpicking Menu",
            "MagicMenu",     "Main Menu",     "MapMenu",       "MessageBoxMenu", "RaceSex Menu",  "Sleep/Wait Menu",
            "StatsMenu",     "Training Menu", "TweenMenu",     "Tutorial Menu"};
        for (const auto name : blockingMenus) {
            if (ui->IsMenuOpen(name.data())) return true;
        }
        return false;
    }
}

void TearsWidget::Init() {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::error("[TearsWidget] UI singleton unavailable, widget menu not registered.");
        return;
    }
    ui->Register(TearsWidgetMenu::MENU_NAME, TearsWidgetMenu::Creator);
    logger::info("[TearsWidget] Widget menu registered.");
}

RE::GPtr<RE::GFxMovieView> TearsWidget::GetWidgetMovie() {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    if (ui->IsMenuOpen("Loading Menu")) return nullptr;
    auto menu = ui->GetMenu(TearsWidgetMenu::MENU_NAME);
    if (!menu || !menu->uiMovie) return nullptr;
    return menu->uiMovie;
}

bool TearsWidget::IsReady() { return GetWidgetMovie() != nullptr; }

void TearsWidget::SetVar(const char* member, double value) {
    auto movie = GetWidgetMovie();
    if (!movie) return;
    RE::GFxValue v;
    v.SetNumber(value);
    movie->SetVariable((std::string(WIDGET_ROOT) + member).c_str(), v);
}

void TearsWidget::SetVarBool(const char* member, bool value) {
    auto movie = GetWidgetMovie();
    if (!movie) return;
    RE::GFxValue v;
    v.SetBoolean(value);
    movie->SetVariable((std::string(WIDGET_ROOT) + member).c_str(), v);
}

void TearsWidget::InvokeArg(const char* method, double arg) {
    auto movie = GetWidgetMovie();
    if (!movie) return;
    RE::GFxValue a;
    a.SetNumber(arg);
    movie->Invoke((std::string(WIDGET_ROOT) + method).c_str(), nullptr, &a, 1);
}

void TearsWidget::ApplyWidgetAlpha(int alpha) {
    if (alpha <= 0) {
        SetVarBool("._visible", false);
        return;
    }
    ForceRootVisible(GetWidgetMovie().get());
    SetVarBool("._visible", true);
    SetVar("._alpha", static_cast<double>(alpha));
}

void TearsWidget::StartFade(int targetAlpha) {
    if (s_currentAlpha.load() == targetAlpha) return;

    const int gen = ++s_fadeGen;
    const int start = s_currentAlpha.load();

    std::thread([gen, start, targetAlpha] {
        constexpr int steps = 12;
        for (int i = 1; i <= steps; ++i) {
            if (s_fadeGen.load() != gen) return;
            const int alpha = start + (targetAlpha - start) * i / steps;
            s_currentAlpha.store(alpha);
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([alpha] { TearsWidget::ApplyWidgetAlpha(alpha); });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }).detach();
}

void TearsWidget::ApplyAutoHideVisibility(bool baseShow, int stage) {
    const bool desired = baseShow && (stage >= s_finalStage || s_peekActive.load());
    if (desired == s_autoHideVisible.load()) return;
    if (desired) {
        s_peekStart = std::chrono::steady_clock::now();
    }
    s_autoHideVisible.store(desired);
    StartFade(desired ? 100 : 0);
}

void TearsWidget::Refresh() {
    RefreshSuppression();
    if (!IsReady()) return;

    ForceRootVisible(GetWidgetMovie().get());

    const auto* mgr = WaterNeedManager::GetSingleton();
    const int stage = mgr->GetStage();
    const int frame = std::clamp(stage + 1, 1, 5);
    const bool baseShow =
        Settings::g_hudVisible && !s_menuSuppressed.load() && mgr->IsSystemEnabled() && !mgr->IsPausedForVampire();

    if (Settings::g_widgetAutoHide && s_lastStage != -1 && stage != s_lastStage) {
        if (stage >= s_finalStage) {
            s_peekActive.store(false);
        } else {
            s_peekActive.store(true);
            s_peekStart = std::chrono::steady_clock::now();
        }
    }
    s_lastStage = stage;

    SetPosition(Settings::g_hudX, Settings::g_hudY);
    SetScale(Settings::g_widgetScale);
    InvokeArg(".setBathColorLevel", static_cast<double>(frame));

    if (Settings::g_widgetAutoHide) {
        ApplyAutoHideVisibility(baseShow, stage);
    } else {
        ++s_fadeGen;
        s_currentAlpha.store(baseShow ? 100 : 0);
        s_autoHideVisible.store(baseShow);
        SetVarBool("._visible", baseShow);
        if (baseShow) SetVar("._alpha", 100.0);
    }
}

void TearsWidget::TickAutoHide() {
    if (!Settings::g_widgetAutoHide) return;
    if (!IsReady()) return;

    if (s_peekActive.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s_peekStart).count();
        if (elapsed >= std::clamp(Settings::g_widgetHoldSeconds, 3, 60)) {
            s_peekActive.store(false);
        }
    }

    const auto* mgr = WaterNeedManager::GetSingleton();
    const bool baseShow =
        Settings::g_hudVisible && !s_menuSuppressed.load() && mgr->IsSystemEnabled() && !mgr->IsPausedForVampire();
    ApplyAutoHideVisibility(baseShow, mgr->GetStage());
}

void TearsWidget::SetPosition(int x, int y) {
    Settings::g_hudX = x;
    Settings::g_hudY = y;
    SetVar("._x", static_cast<double>(x));
    SetVar("._y", static_cast<double>(y));
}

void TearsWidget::SetScale(int scalePercent) {
    const int s = std::clamp(scalePercent, 10, 150);
    Settings::g_widgetScale = s;
    SetVar("._xscale", static_cast<double>(s));
    SetVar("._yscale", static_cast<double>(s));
}

void TearsWidget::ShowNotification(const char* message) {
    if (!message || !*message) return;
    RE::DebugNotification(message);
}

void TearsWidget::BeginBinding(const char* type) {
    const std::string t = type ? type : "";
    s_pendingBinding = (t == "fill" || t == "togglewidget") ? t : std::string{};
}

void TearsWidget::CancelBinding() { s_pendingBinding.clear(); }

bool TearsWidget::IsAwaitingBinding() { return !s_pendingBinding.empty(); }

const std::string& TearsWidget::GetPendingBinding() { return s_pendingBinding; }

bool TearsWidget::CaptureBindingKey(std::uint32_t scanCode) {
    if (s_pendingBinding.empty()) return false;
    if (scanCode == 0x01) {
        CancelBinding();
        return true;
    }

    if (s_pendingBinding == "fill") {
        Settings::g_fillKey = scanCode;
    } else if (s_pendingBinding == "togglewidget") {
        Settings::g_toggleWidgetKey = scanCode;
    } else {
        s_pendingBinding.clear();
        return false;
    }

    s_pendingBinding.clear();
    Settings::SaveToINI();
    Hotkeys::RefreshBindings();
    return true;
}

void TearsWidget::RefreshSuppression() {
    EnsureMenuShown();

    if (!s_inGameSession.load() && !s_sessionPending.load()) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (player && ui && player->Is3DLoaded() && player->GetParentCell() && !ui->IsMenuOpen("Main Menu") &&
            !ui->IsMenuOpen("Loading Menu") && !ui->IsMenuOpen("RaceSex Menu")) {
            s_sessionPending.store(true);
            s_requireRaceMenuExit.store(false);
            s_raceMenuSeen.store(false);
        }
    }

    if (!s_inGameSession.load() && s_sessionPending.load()) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (player && ui && player->Is3DLoaded() && player->GetParentCell() && !ui->IsMenuOpen("Main Menu") &&
            !ui->IsMenuOpen("Loading Menu")) {
            bool ok = true;
            if (s_requireRaceMenuExit.load()) {
                if (ui->IsMenuOpen("RaceSex Menu")) {
                    s_raceMenuSeen.store(true);
                    ok = false;
                } else if (!s_raceMenuSeen.load())
                    ok = false;
            }
            if (ok) {
                s_inGameSession.store(true);
                s_sessionPending.store(false);
                s_requireRaceMenuExit.store(false);
                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([] { WaterskinUtils::OnInGameSessionReady(); });
                } else {
                    WaterskinUtils::OnInGameSessionReady();
                }
                logger::info("[TearsWidget] In-game session ready.");
            }
        }
    }

    s_menuSuppressed.store(!s_inGameSession.load() || ShouldSuppressForMenus());
}

void TearsWidget::BeginGameSessionTransition(bool requireRaceMenuExit) {
    s_sessionPending.store(true);
    s_inGameSession.store(false);
    s_requireRaceMenuExit.store(requireRaceMenuExit);
    s_raceMenuSeen.store(false);
    ResetAutoHideState();
    RefreshSuppression();
}

void TearsWidget::NotifyMenuEvent(const char* menuName, bool opening) {
    const std::string_view name = menuName ? menuName : "";
    if (name == "HUD Menu") {
        if (opening) {
            EnsureMenuShown();
        } else {
            HideMenu();
        }
    }
    if (opening && name == "Main Menu") {
        EndGameSession();
        return;
    }
    if (opening && name == "Loading Menu") {
        ResetAutoHideState();
        RefreshSuppression();
        return;
    }
    if (s_sessionPending.load() && s_requireRaceMenuExit.load() && name == "RaceSex Menu" && opening) {
        s_raceMenuSeen.store(true);
    }
    RefreshSuppression();
    Refresh();
}

void TearsWidget::EndGameSession(bool cancelPendingWaterskin) {
    s_sessionPending.store(false);
    s_inGameSession.store(false);
    s_requireRaceMenuExit.store(false);
    s_raceMenuSeen.store(false);
    ResetAutoHideState();
    if (cancelPendingWaterskin) WaterskinUtils::CancelPendingStartingWaterskin();
    RefreshSuppression();
}

bool TearsWidget::IsInGameSession() { return s_inGameSession.load(); }
