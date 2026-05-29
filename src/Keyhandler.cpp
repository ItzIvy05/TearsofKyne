#include "Keyhandler.h"

#include "MCP.h"

KeyHandler* KeyHandler::GetSingleton()
{
    static KeyHandler instance;
    return &instance;
}

void KeyHandler::RegisterSink()
{
    static std::atomic_bool registered = false;
    if (registered.load()) {
        return;
    }

    auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputManager) {
        logger::error("[KeyHandler] Could not get BSInputDeviceManager.");
        return;
    }

    if (!registered.exchange(true)) {
        inputManager->AddEventSink(GetSingleton());
        logger::info("[KeyHandler] Registered input sink.");
    }
}

KeyHandlerEvent KeyHandler::Register(std::uint32_t scanCode, KeyEventType eventType, KeyCallback callback)
{
    if (!callback) {
        return INVALID_KEY_HANDLER_EVENT;
    }

    const auto handle = _nextHandle.fetch_add(1);
    if (handle == INVALID_KEY_HANDLER_EVENT) {
        return INVALID_KEY_HANDLER_EVENT;
    }

    std::unique_lock lock(_mutex);
    auto& callbacks = _registeredCallbacks[scanCode];
    auto& target = eventType == KeyEventType::KEY_DOWN ? callbacks.down : callbacks.up;
    target[handle] = std::move(callback);
    _handleMap[handle] = { scanCode, eventType };
    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle)
{
    if (handle == INVALID_KEY_HANDLER_EVENT) {
        return;
    }

    std::unique_lock lock(_mutex);
    const auto handleIt = _handleMap.find(handle);
    if (handleIt == _handleMap.end()) {
        return;
    }

    const auto info = handleIt->second;
    _handleMap.erase(handleIt);

    const auto keyIt = _registeredCallbacks.find(info.key);
    if (keyIt == _registeredCallbacks.end()) {
        return;
    }

    auto& callbacks = keyIt->second;
    auto& target = info.type == KeyEventType::KEY_DOWN ? callbacks.down : callbacks.up;
    target.erase(handle);

    if (callbacks.down.empty() && callbacks.up.empty()) {
        _registeredCallbacks.erase(keyIt);
    }
}

RE::BSEventNotifyControl KeyHandler::ProcessEvent(RE::InputEvent* const* eventList, RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!eventList) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->GameIsPaused()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const bool prismaFocused = [] {
        if (auto* prismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>(); prismaUI) {
            return prismaUI->HasAnyActiveFocus();
        }
        return false;
    }();

    std::vector<KeyCallback> callbacksToRun;

    for (auto* event = *eventList; event; event = event->next) {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        const auto* button = event->AsButtonEvent();
        if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
            continue;
        }

        const auto scanCode = button->GetIDCode();
        KeyEventType type;
        if (button->IsDown()) {
            type = KeyEventType::KEY_DOWN;
        } else if (button->IsUp()) {
            type = KeyEventType::KEY_UP;
        } else {
            continue;
        }

        auto* hudManager = HUDManager::GetSingleton();
        if (prismaFocused) {
            if (type == KeyEventType::KEY_DOWN) {
                if (hudManager->CaptureBindingKey(scanCode)) {
                    continue;
                }

                if (hudManager->IsSettingsMenuOpen() && (scanCode == Settings::g_menuKey || scanCode == 0x01)) {
                    auto* taskInterface = SKSE::GetTaskInterface();
                    if (taskInterface) {
                        taskInterface->AddTask([] { HUDManager::GetSingleton()->CloseSettingsMenu(); });
                    }
                    continue;
                }
            }

            continue;
        }

        std::shared_lock lock(_mutex);
        const auto keyIt = _registeredCallbacks.find(scanCode);
        if (keyIt == _registeredCallbacks.end()) {
            continue;
        }

        const auto& callbacks = keyIt->second;
        const auto& source = type == KeyEventType::KEY_DOWN ? callbacks.down : callbacks.up;
        for (const auto& [_, callback] : source) {
            callbacksToRun.push_back(callback);
        }
    }

    for (const auto& callback : callbacksToRun) {
        callback();
    }

    return RE::BSEventNotifyControl::kContinue;
}
