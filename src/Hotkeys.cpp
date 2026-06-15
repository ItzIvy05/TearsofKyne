#include "Hotkeys.h"

#include "Keyhandler.h"
#include "Menu.h"
#include "Utils.h"

namespace Hotkeys {
    namespace {
        KeyHandlerEvent g_fillHandle = INVALID_KEY_HANDLER_EVENT;
        KeyHandlerEvent g_toggleWidgetHandle = INVALID_KEY_HANDLER_EVENT;

        void UnregisterAll() {
            auto* keyHandler = KeyHandler::GetSingleton();
            keyHandler->Unregister(g_fillHandle);
            keyHandler->Unregister(g_toggleWidgetHandle);
            g_fillHandle = INVALID_KEY_HANDLER_EVENT;
            g_toggleWidgetHandle = INVALID_KEY_HANDLER_EVENT;
        }

        void RegisterAll() {
            auto* keyHandler = KeyHandler::GetSingleton();

            g_fillHandle = keyHandler->Register(Settings::g_fillKey, KeyEventType::KEY_DOWN, [] {
                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([] { WaterskinUtils::TryFill(); });
                }
            });

            if (Settings::g_toggleWidgetKey != 0) {
                g_toggleWidgetHandle = keyHandler->Register(Settings::g_toggleWidgetKey, KeyEventType::KEY_DOWN, [] {
                    auto* taskInterface = SKSE::GetTaskInterface();
                    if (taskInterface) {
                        taskInterface->AddTask([] {
                            Settings::g_hudVisible = !Settings::g_hudVisible;
                            Settings::SaveToINI();
                            TearsWidget::Refresh();
                        });
                    }
                });
            }
        }
    }

    void Initialize() {
        KeyHandler::RegisterSink();
        RefreshBindings();
    }

    void RefreshBindings() {
        UnregisterAll();
        RegisterAll();
        logger::info("[Hotkeys] Registered. FillKey={} ToggleWidgetKey={}", Settings::GetKeyName(Settings::g_fillKey),
                     Settings::GetKeyName(Settings::g_toggleWidgetKey));
    }
}