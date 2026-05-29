#pragma once

class HUDManager {
public:
    static HUDManager* GetSingleton();

    void Initialize(PRISMA_UI_API::IVPrismaUI1* api);
    void PushUpdate();
    void RefreshSettingsMenu();
    void ShowNotification(const char* message);

    void OpenSettingsMenu();
    void CloseSettingsMenu();
    void ToggleSettingsMenu();
    [[nodiscard]] bool CaptureBindingKey(std::uint32_t scanCode);
    void RefreshMenuSuppression();
    void BeginGameSessionTransition(bool requireRaceMenuExit);
    void NotifyMenuEvent(const char* menuName, bool opening);
    void EndGameSession(bool cancelPendingStartingWaterskin = true);

    [[nodiscard]] bool IsInGameSession() const { return _inGameSession.load(); }

    [[nodiscard]] bool IsReady() const { return _api != nullptr && _domReady.load(); }
    [[nodiscard]] bool IsSettingsMenuOpen() const { return _settingsOpen.load(); }
    [[nodiscard]] bool IsMenuSuppressed() const { return _menuSuppressed.load(); }

private:
    HUDManager() = default;

    void HandleBeginBinding(const char* data);
    void HandleCancelBinding();
    void HandleApplyBinding(const char* data);
    void HandleSaveHudPosition(const char* data);
    void HandleResetHudPosition();
    void HandleSetHudVisible(const char* data);
    void HandleSaveMenuPosition(const char* data);
    void HandleSetWidgetStyle(const char* data);
    void HandleSetDifficulty(const char* data);
    void HandleSetAccessibilityTheme(const char* data);
    void HandleSetWidgetScale(const char* data);
    void HandleSetWaterSystemEnabled(const char* data);
    void SetMenuSuppressed(bool suppressed);
    void UpdateInGameSessionState();

    PRISMA_UI_API::IVPrismaUI1* _api = nullptr;
    PrismaView _view = 0;
    std::atomic<bool> _domReady = false;
    std::atomic<bool> _settingsOpen = false;
    std::atomic<bool> _menuSuppressed = false;
    std::atomic<bool> _sessionPending = false;
    std::atomic<bool> _inGameSession = false;
    std::atomic<bool> _requireRaceMenuExit = false;
    std::atomic<bool> _raceMenuSeen = false;
    std::string _pendingBinding;
};
