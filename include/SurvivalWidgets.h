#pragma once

namespace SurvivalWidgets {
    void Init();
    void Notify();
    void Refresh();
    void ResetSession();
    void TickAutoHide();
    void NotifyMenuEvent(const char* menuName, bool opening);

    [[nodiscard]] bool IsAvailable();
    [[nodiscard]] bool IsNeedAvailable(int index);
    [[nodiscard]] const char* GetLabel(int index);

    void SetPosition(int index, int x, int y);
    void SetScale(int index, int scalePercent);
}
