#include "Hotkeys.h"
#include "Keyhandler.h"
#include "MCP.h"
#include "Utils.h"

namespace Hotkeys {
    namespace {
        KeyHandlerEvent g_fillHandle = INVALID_KEY_HANDLER_EVENT;
        KeyHandlerEvent g_menuHandle = INVALID_KEY_HANDLER_EVENT;
        KeyHandlerEvent g_toggleHUDHandle = INVALID_KEY_HANDLER_EVENT;

        void UnregisterAll()
        {
            auto* keyHandler = KeyHandler::GetSingleton();
            keyHandler->Unregister(g_fillHandle);
            keyHandler->Unregister(g_menuHandle);
            keyHandler->Unregister(g_toggleHUDHandle);
            g_fillHandle = INVALID_KEY_HANDLER_EVENT;
            g_menuHandle = INVALID_KEY_HANDLER_EVENT;
            g_toggleHUDHandle = INVALID_KEY_HANDLER_EVENT;
        }

        void RegisterAll()
        {
            auto* keyHandler = KeyHandler::GetSingleton();

            g_fillHandle = keyHandler->Register(Settings::g_fillKey, KeyEventType::KEY_DOWN, [] {
                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([] { WaterskinUtils::TryFill(); });
                }
            });


            g_menuHandle = keyHandler->Register(Settings::g_menuKey, KeyEventType::KEY_DOWN, [] {
                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([] { HUDManager::GetSingleton()->ToggleSettingsMenu(); });
                }
            });

            g_toggleHUDHandle = keyHandler->Register(Settings::g_toggleHUDKey, KeyEventType::KEY_DOWN, [] {
                auto* taskInterface = SKSE::GetTaskInterface();
                if (taskInterface) {
                    taskInterface->AddTask([] {
                        Settings::g_hudVisible = !Settings::g_hudVisible;
                        Settings::SaveToINI();
                        HUDManager::GetSingleton()->RefreshSettingsMenu();
                        HUDManager::GetSingleton()->PushUpdate();
                    });
                }
            });
        }
    }

    void Initialize()
    {
        KeyHandler::RegisterSink();
        RefreshBindings();
    }

    void RefreshBindings()
    {
        UnregisterAll();
        RegisterAll();
        logger::info(
            "[Hotkeys] Registered. FillKey={} MenuKey={} ToggleHUDKey={}",
            Settings::GetKeyName(Settings::g_fillKey),
            Settings::GetKeyName(Settings::g_menuKey),
            Settings::GetKeyName(Settings::g_toggleHUDKey));
    }
}
