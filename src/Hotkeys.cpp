#include "Hotkeys.h"

#include "Menu.h"
#include "SurvivalWidgets.h"
#include "Utils.h"

namespace Hotkeys {
    namespace {
        void ToggleWidget() {
            Settings::g_hudVisible = !Settings::g_hudVisible;
            Settings::SaveToINI();
            TearsWidget::Refresh();
            SurvivalWidgets::Refresh();
        }

        class InputSink final : public RE::BSTEventSink<RE::InputEvent*>, public REX::Singleton<InputSink> {
        public:
            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override {
                auto* ui = RE::UI::GetSingleton();
                if (!a_events || (ui && ui->GameIsPaused())) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                for (auto* event = *a_events; event; event = event->next) {
                    const auto* button = event->AsButtonEvent();
                    if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) {
                        continue;
                    }

                    const auto key = button->GetIDCode();
                    if (key == Settings::g_fillKey && Settings::g_fillMethod == Settings::FillMethod::Hotkey) {
                        SKSE::GetTaskInterface()->AddTask([] { WaterskinUtils::TryFill(); });
                    }
                    if (key == Settings::g_toggleWidgetKey && Settings::g_toggleWidgetKey != 0) {
                        SKSE::GetTaskInterface()->AddTask(ToggleWidget);
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void Initialize() {
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(InputSink::GetSingleton());
        logger::info("[Hotkeys] Registered input sink.");
    }
}
