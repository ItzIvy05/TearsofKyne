#include "Menu.h"

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

    int s_widgetIndex = -1;
    int s_appearanceApplyCount = 0;
    std::chrono::steady_clock::time_point s_lastAppearanceApply{};
    std::string s_widgetRoot;

    constexpr const char* HUD_MENU = "HUD Menu";

    RE::TESGlobal* g_indexGlobal = nullptr;
    RE::TESGlobal* g_readyGlobal = nullptr;

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
    g_indexGlobal = RE::TESForm::LookupByEditorID<RE::TESGlobal>("TearsWidgetIndex");
    g_readyGlobal = RE::TESForm::LookupByEditorID<RE::TESGlobal>("TearsWidgetReady");
    if (!g_indexGlobal || !g_readyGlobal) {
        logger::warn("[TearsWidget] TearsWidgetIndex/TearsWidgetReady globals not found.");
    } else {
        logger::info("[TearsWidget] Globals resolved.");
    }
}

RE::GPtr<RE::GFxMovieView> TearsWidget::GetHudMovie() {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    auto menu = ui->GetMenu(HUD_MENU);
    if (!menu || !menu->uiMovie) return nullptr;
    return menu->uiMovie;
}

bool TearsWidget::ResolveWidget() {
    if (s_widgetIndex >= 0 && !s_widgetRoot.empty()) return true;
    if (!g_readyGlobal || !g_indexGlobal) return false;
    if (g_readyGlobal->value < 0.5f) return false;

    const int idx = static_cast<int>(g_indexGlobal->value + 0.5f);
    if (idx < 0) return false;

    s_widgetIndex = idx;
    s_widgetRoot = std::format("_root.WidgetContainer.{}.widget", idx);
    s_appearanceApplyCount = 10;
    logger::info("[TearsWidget] Resolved widget root '{}'.", s_widgetRoot);
    return true;
}

bool TearsWidget::IsReady() { return ResolveWidget() && GetHudMovie() != nullptr; }

void TearsWidget::SetVar(const char* member, double value) {
    if (!ResolveWidget()) return;
    auto movie = GetHudMovie();
    if (!movie) return;
    RE::GFxValue v;
    v.SetNumber(value);
    movie->SetVariable((s_widgetRoot + member).c_str(), v);
}

void TearsWidget::SetVarBool(const char* member, bool value) {
    if (!ResolveWidget()) return;
    auto movie = GetHudMovie();
    if (!movie) return;
    RE::GFxValue v;
    v.SetBoolean(value);
    movie->SetVariable((s_widgetRoot + member).c_str(), v);
}

void TearsWidget::InvokeArg(const char* method, double arg) {
    if (!ResolveWidget()) return;
    auto movie = GetHudMovie();
    if (!movie) return;
    RE::GFxValue a;
    a.SetNumber(arg);
    movie->Invoke((s_widgetRoot + method).c_str(), nullptr, &a, 1);
}

void TearsWidget::ApplyPendingAppearance() {
    if (s_appearanceApplyCount <= 0) return;
    if (!IsReady()) return;

    const auto now = std::chrono::steady_clock::now();
    if (s_appearanceApplyCount < 10) {
        const auto sinceLast =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastAppearanceApply).count();
        if (sinceLast < 900) return;
    }
    s_lastAppearanceApply = now;
    s_appearanceApplyCount--;

    SetPosition(Settings::g_hudX, Settings::g_hudY);
    SetScale(Settings::g_widgetScale);
}

void TearsWidget::Refresh() {
    RefreshSuppression();
    if (!IsReady()) return;

    const auto* mgr = WaterNeedManager::GetSingleton();
    const int percent = std::clamp(mgr->GetPercent(), 0, 100);
    const int frame = std::clamp(percent / 10 + 1, 1, 10);
    const bool show =
        Settings::g_hudVisible && !s_menuSuppressed.load() && mgr->IsSystemEnabled() && !mgr->IsPausedForVampire();

    SetVarBool("._visible", show);
    if (show) {
        InvokeArg(".setBathColorLevel", static_cast<double>(frame));
        SetVar("._alpha", 100.0);
    }
}

void TearsWidget::SetPosition(int x, int y) {
    Settings::g_hudX = x;
    Settings::g_hudY = y;
    InvokeArg(".setPositionX", static_cast<double>(x));
    InvokeArg(".setPositionY", static_cast<double>(y));
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
    s_widgetIndex = -1;
    s_widgetRoot.clear();
    RefreshSuppression();
}

void TearsWidget::NotifyMenuEvent(const char* menuName, bool opening) {
    const std::string_view name = menuName ? menuName : "";
    if (opening && name == "Main Menu") {
        EndGameSession();
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
    s_widgetIndex = -1;
    s_widgetRoot.clear();
    if (cancelPendingWaterskin) WaterskinUtils::CancelPendingStartingWaterskin();
    RefreshSuppression();
}

bool TearsWidget::IsInGameSession() { return s_inGameSession.load(); }
bool TearsWidget::IsMenuSuppressed() { return s_menuSuppressed.load(); }