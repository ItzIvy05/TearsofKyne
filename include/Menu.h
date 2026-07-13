#pragma once

class TearsWidget {
public:
    static void Init();
    static void Refresh();
    static void TickAutoHide();
    static void SetPosition(int x, int y);
    static void SetScale(int scalePercent);

    static void BeginGameSessionTransition(bool requireRaceMenuExit);
    static void NotifyMenuEvent(const char* menuName, bool opening);
    static void EndGameSession(bool cancelPendingWaterskin = true);
    static void RefreshSuppression();

    static void ShowNotification(const char* message);

    static bool CaptureBindingKey(std::uint32_t scanCode);
    static void BeginBinding(const char* type);
    static void CancelBinding();
    [[nodiscard]] static bool IsAwaitingBinding();
    [[nodiscard]] static const std::string& GetPendingBinding();

    [[nodiscard]] static bool IsInGameSession();

private:
    [[nodiscard]] static bool IsReady();
    [[nodiscard]] static RE::GPtr<RE::GFxMovieView> GetWidgetMovie();
    static void SetVar(const char* member, double value);
    static void SetVarBool(const char* member, bool value);
    static void InvokeArg(const char* method, double arg);

    static void ApplyWidgetAlpha(int alpha);
    static void StartFade(int targetAlpha);
    static void ApplyAutoHideVisibility(bool baseShow, int stage);
};
