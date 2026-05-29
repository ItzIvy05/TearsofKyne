#include "MCP.h"

#include <array>

#include "Hotkeys.h"
#include "Manager.h"
#include "Utils.h"

namespace {
    std::vector<std::string> Split(std::string_view text, char separator) {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= text.size()) {
            const auto found = text.find(separator, start);
            if (found == std::string_view::npos) {
                parts.emplace_back(text.substr(start));
                break;
            }
            parts.emplace_back(text.substr(start, found - start));
            start = found + 1;
        }
        return parts;
    }

    int ParseInt(std::string_view text, int fallback) {
        try {
            return std::stoi(std::string(text));
        } catch (...) {
            return fallback;
        }
    }

    bool ShouldSuppressWidgetForMenus() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || !player->GetParentCell()) {
            return true;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return true;
        }

        static constexpr std::array<std::string_view, 22> blockingMenus = {
            "BarterMenu",    "Book Menu",     "Console",       "ContainerMenu",  "Crafting Menu", "Dialogue Menu",
            "FavoritesMenu", "GiftMenu",      "InventoryMenu", "Journal Menu",   "Loading Menu",  "Lockpicking Menu",
            "MagicMenu",     "Main Menu",     "MapMenu",       "MessageBoxMenu", "RaceSex Menu",  "Sleep/Wait Menu",
            "StatsMenu",     "Training Menu", "TweenMenu",     "Tutorial Menu"};

        for (const auto menuName : blockingMenus) {
            if (ui->IsMenuOpen(menuName.data())) {
                return true;
            }
        }

        return false;
    }
}

HUDManager* HUDManager::GetSingleton() {
    static HUDManager instance;
    return &instance;
}

void HUDManager::Initialize(PRISMA_UI_API::IVPrismaUI1* api) {
    _api = api;
    _sessionPending.store(false);
    _inGameSession.store(false);
    _menuSuppressed.store(true);

    if (!_api) {
        logger::warn("[HUD] PrismaUI API not available. Falling back to notifications only.");
        return;
    }

    _view = _api->CreateView(Settings::UI_VIEW_PATH, [](PrismaView) {
        auto* manager = HUDManager::GetSingleton();
        manager->_domReady.store(true);
        manager->RefreshMenuSuppression();
        manager->_api->InteropCall(manager->_view, "setWidgetMenuSuppressed",
                                   manager->_menuSuppressed.load() ? "1" : "0");
        manager->PushUpdate();
        manager->RefreshSettingsMenu();
        manager->_api->InteropCall(manager->_view, "setSettingsMenuOpen", "0");
        logger::info("[HUD] DOM ready.");
    });

    if (_view == 0) {
        logger::error("[HUD] Failed to create PrismaUI view '{}'.", Settings::UI_VIEW_PATH);
        return;
    }

    _api->RegisterJSListener(_view, "tokCloseSettings",
                             [](const char*) { HUDManager::GetSingleton()->CloseSettingsMenu(); });

    _api->RegisterJSListener(_view, "tokRequestSettings",
                             [](const char*) { HUDManager::GetSingleton()->RefreshSettingsMenu(); });

    _api->RegisterJSListener(_view, "tokBeginBinding", [](const char* data) {
        HUDManager::GetSingleton()->HandleBeginBinding(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokCancelBinding",
                             [](const char*) { HUDManager::GetSingleton()->HandleCancelBinding(); });

    _api->RegisterJSListener(_view, "tokApplyBinding", [](const char* data) {
        HUDManager::GetSingleton()->HandleApplyBinding(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSaveHudPosition", [](const char* data) {
        HUDManager::GetSingleton()->HandleSaveHudPosition(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokResetHudPosition",
                             [](const char*) { HUDManager::GetSingleton()->HandleResetHudPosition(); });

    _api->RegisterJSListener(_view, "tokSetHudVisible", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetHudVisible(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSaveMenuPosition", [](const char* data) {
        HUDManager::GetSingleton()->HandleSaveMenuPosition(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSetWidgetStyle", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetWidgetStyle(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSetDifficulty", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetDifficulty(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSetAccessibilityTheme", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetAccessibilityTheme(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSetWidgetScale", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetWidgetScale(data ? data : "");
    });

    _api->RegisterJSListener(_view, "tokSetWaterSystemEnabled", [](const char* data) {
        HUDManager::GetSingleton()->HandleSetWaterSystemEnabled(data ? data : "");
    });

    logger::info("[HUD] View {} created.", _view);
}

void HUDManager::PushUpdate() {
    if (!IsReady()) {
        return;
    }

    RefreshMenuSuppression();

    const auto* manager = WaterNeedManager::GetSingleton();
    const auto payload = std::format(
        "{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}", manager->GetPercent(), manager->GetStage(),
        manager->GetStageName(), WaterskinUtils::HasFilledWaterskin() ? 1 : 0, WaterskinUtils::IsNearWater() ? 1 : 0,
        Settings::DIFFICULTY_NAMES[static_cast<int>(Settings::g_difficulty)], Settings::GetKeyName(Settings::g_fillKey),
        Settings::GetKeyName(Settings::g_menuKey), Settings::GetKeyName(Settings::g_toggleHUDKey), Settings::g_hudX,
        Settings::g_hudY, Settings::g_hudVisible ? 1 : 0, static_cast<int>(Settings::g_widgetStyle),
        Settings::g_widgetScale, static_cast<int>(Settings::g_accessibilityTheme), manager->IsSystemEnabled() ? 1 : 0);

    _api->InteropCall(_view, "updateWaterHUD", payload.c_str());
}

void HUDManager::RefreshSettingsMenu() {
    if (!IsReady()) {
        return;
    }

    const auto payload = std::format(
        "{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}", Settings::GetKeyName(Settings::g_fillKey),
        Settings::GetKeyName(Settings::g_menuKey), Settings::GetKeyName(Settings::g_toggleHUDKey), Settings::g_hudX,
        Settings::g_hudY, Settings::g_hudVisible ? 1 : 0, static_cast<int>(Settings::g_widgetStyle),
        static_cast<int>(Settings::g_difficulty), Settings::g_menuX, Settings::g_menuY, Settings::g_widgetScale,
        static_cast<int>(Settings::g_accessibilityTheme), WaterNeedManager::GetSingleton()->IsSystemEnabled() ? 1 : 0);

    _api->InteropCall(_view, "updateSettingsMenu", payload.c_str());
}

void HUDManager::ShowNotification(const char* message) {
    if (!message || !*message) {
        return;
    }

    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
        RE::DebugNotification(message);
        return;
    }

    if (!IsReady()) {
        RE::DebugNotification(message);
        return;
    }

    _api->InteropCall(_view, "showWaterNotification", message);
}

void HUDManager::OpenSettingsMenu() {
    RefreshMenuSuppression();

    if (!IsReady() || _menuSuppressed.load() || _settingsOpen.exchange(true)) {
        return;
    }

    RefreshSettingsMenu();
    _api->Focus(_view, false, false);
    _api->InteropCall(_view, "setSettingsMenuOpen", "1");
}

void HUDManager::CloseSettingsMenu() {
    _pendingBinding.clear();

    if (!_api || _view == 0 || !_settingsOpen.exchange(false)) {
        return;
    }

    if (_api->HasFocus(_view)) {
        _api->Unfocus(_view);
    }
    _api->InteropCall(_view, "setSettingsMenuOpen", "0");
    PushUpdate();
}

void HUDManager::ToggleSettingsMenu() {
    if (_settingsOpen.load()) {
        CloseSettingsMenu();
    } else {
        OpenSettingsMenu();
    }
}

void HUDManager::BeginGameSessionTransition(bool requireRaceMenuExit) {
    _sessionPending.store(true);
    _inGameSession.store(false);
    _requireRaceMenuExit.store(requireRaceMenuExit);
    _raceMenuSeen.store(false);
    RefreshMenuSuppression();
}

void HUDManager::NotifyMenuEvent(const char* menuName, bool opening) {
    const std::string_view name = menuName ? menuName : "";

    if (opening && name == "Main Menu") {
        EndGameSession();
        return;
    }

    if (_sessionPending.load() && _requireRaceMenuExit.load() && name == "RaceSex Menu" && opening) {
        _raceMenuSeen.store(true);
    }

    RefreshMenuSuppression();
}

void HUDManager::EndGameSession(bool cancelPendingStartingWaterskin) {
    _sessionPending.store(false);
    _inGameSession.store(false);
    _requireRaceMenuExit.store(false);
    _raceMenuSeen.store(false);
    if (cancelPendingStartingWaterskin) {
        WaterskinUtils::CancelPendingStartingWaterskin();
    }

    if (_settingsOpen.load()) {
        CloseSettingsMenu();
    }

    RefreshMenuSuppression();
}

void HUDManager::UpdateInGameSessionState() {
    if (_inGameSession.load() || !_sessionPending.load()) {
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* ui = RE::UI::GetSingleton();
    if (!player || !ui) {
        return;
    }

    if (!player->Is3DLoaded() || !player->GetParentCell()) {
        return;
    }

    if (ui->IsMenuOpen("Main Menu") || ui->IsMenuOpen("Loading Menu")) {
        return;
    }

    const bool raceMenuOpen = ui->IsMenuOpen("RaceSex Menu");
    if (_requireRaceMenuExit.load()) {
        if (raceMenuOpen) {
            _raceMenuSeen.store(true);
            return;
        }
        if (!_raceMenuSeen.load()) {
            return;
        }
    }

    _inGameSession.store(true);
    _sessionPending.store(false);
    _requireRaceMenuExit.store(false);

    if (auto* taskInterface = SKSE::GetTaskInterface()) {
        taskInterface->AddTask([] { WaterskinUtils::OnInGameSessionReady(); });
    } else {
        WaterskinUtils::OnInGameSessionReady();
    }

    logger::info("[HUD] In-game session is now ready.");
}

void HUDManager::SetMenuSuppressed(bool suppressed) {
    if (_menuSuppressed.exchange(suppressed) == suppressed) {
        return;
    }

    if (IsReady()) {
        _api->InteropCall(_view, "setWidgetMenuSuppressed", suppressed ? "1" : "0");
    }
}

void HUDManager::RefreshMenuSuppression() {
    if (!_inGameSession.load() && !_sessionPending.load()) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (player && ui && player->Is3DLoaded() && player->GetParentCell() && !ui->IsMenuOpen("Main Menu") &&
            !ui->IsMenuOpen("Loading Menu") && !ui->IsMenuOpen("RaceSex Menu")) {
            _sessionPending.store(true);
            _requireRaceMenuExit.store(false);
            _raceMenuSeen.store(false);
        }
    }

    UpdateInGameSessionState();
    SetMenuSuppressed(!_inGameSession.load() || ShouldSuppressWidgetForMenus());
}

bool HUDManager::CaptureBindingKey(std::uint32_t scanCode) {
    if (!_settingsOpen.load() || _pendingBinding.empty()) {
        return false;
    }

    if (scanCode == 0x01) {
        HandleCancelBinding();
        return true;
    }

    if (_pendingBinding == "fill") {
        Settings::g_fillKey = scanCode;
    } else if (_pendingBinding == "menu") {
        Settings::g_menuKey = scanCode;
    } else if (_pendingBinding == "togglehud") {
        Settings::g_toggleHUDKey = scanCode;
    } else {
        _pendingBinding.clear();
        return false;
    }

    _pendingBinding.clear();

    if (IsReady()) {
        _api->InteropCall(_view, "finishBinding", "");
    }

    Settings::SaveToINI();
    Hotkeys::RefreshBindings();
    RefreshSettingsMenu();
    PushUpdate();
    return true;
}

void HUDManager::HandleBeginBinding(const char* data) {
    const std::string binding = data ? data : "";
    if (binding == "fill" || binding == "menu" || binding == "togglehud") {
        _pendingBinding = binding;
    } else {
        _pendingBinding.clear();
    }
}

void HUDManager::HandleCancelBinding() {
    if (_pendingBinding.empty()) {
        return;
    }

    _pendingBinding.clear();
    if (IsReady()) {
        _api->InteropCall(_view, "cancelBinding", "");
    }
    RefreshSettingsMenu();
}

void HUDManager::HandleApplyBinding(const char* data) {
    if (!data || !*data) {
        return;
    }

    const auto parts = Split(data, '|');
    if (parts.size() < 2) {
        return;
    }

    std::uint32_t scanCode = 0;
    if (!Settings::TryParseKeyName(parts[1], scanCode)) {
        return;
    }

    _pendingBinding = parts[0];
    CaptureBindingKey(scanCode);
}

void HUDManager::HandleSaveHudPosition(const char* data) {
    if (!data || !*data) {
        return;
    }

    const auto parts = Split(data, '|');
    if (parts.size() < 2) {
        return;
    }

    Settings::g_hudX = std::max(0, ParseInt(parts[0], Settings::g_hudX));
    Settings::g_hudY = std::max(0, ParseInt(parts[1], Settings::g_hudY));
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleResetHudPosition() {
    Settings::g_hudX = Settings::DEFAULT_HUD_X;
    Settings::g_hudY = Settings::DEFAULT_HUD_Y;
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSetHudVisible(const char* data) {
    Settings::g_hudVisible = data && data[0] == '1';
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSaveMenuPosition(const char* data) {
    if (!data || !*data) {
        return;
    }

    const auto parts = Split(data, '|');
    if (parts.size() < 2) {
        return;
    }

    Settings::g_menuX = ParseInt(parts[0], Settings::g_menuX);
    Settings::g_menuY = ParseInt(parts[1], Settings::g_menuY);
    Settings::SaveToINI();
    RefreshSettingsMenu();
}

void HUDManager::HandleSetWidgetStyle(const char* data) {
    const int style = ParseInt(data ? data : "0", 0);
    Settings::g_widgetStyle = style == 1
                                  ? Settings::WidgetStyle::Simple
                                  : (style == 2 ? Settings::WidgetStyle::Nordic : Settings::WidgetStyle::Detailed);
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSetDifficulty(const char* data) {
    const int difficulty = std::clamp(ParseInt(data ? data : "1", 1), 0, 3);
    Settings::ApplyDifficulty(static_cast<Settings::Difficulty>(difficulty));
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSetAccessibilityTheme(const char* data) {
    const int theme = std::clamp(ParseInt(data ? data : "0", 0), 0, 5);
    Settings::ApplyAccessibilityTheme(static_cast<Settings::AccessibilityTheme>(theme));
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSetWidgetScale(const char* data) {
    Settings::g_widgetScale = std::clamp(ParseInt(data ? data : "100", Settings::g_widgetScale), 50, 150);
    Settings::SaveToINI();
    RefreshSettingsMenu();
    PushUpdate();
}

void HUDManager::HandleSetWaterSystemEnabled(const char* data) {
    const bool enabled = data && data[0] == '1';
    WaterskinUtils::SetSystemEnabled(enabled);
}
