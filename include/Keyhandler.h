#pragma once

using KeyCallback = std::function<void()>;
using KeyHandlerEvent = std::uint64_t;

inline constexpr KeyHandlerEvent INVALID_KEY_HANDLER_EVENT = 0;

enum class KeyEventType : std::uint8_t {
    KEY_DOWN,
    KEY_UP
};

struct CallbackInfo {
    std::uint32_t key = 0;
    KeyEventType type = KeyEventType::KEY_DOWN;
};

class KeyHandler : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static KeyHandler* GetSingleton();
    static void RegisterSink();

    [[nodiscard]] KeyHandlerEvent Register(std::uint32_t scanCode, KeyEventType eventType, KeyCallback callback);
    void Unregister(KeyHandlerEvent handle);

private:
    KeyHandler() = default;
    ~KeyHandler() override = default;

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* eventList, RE::BSTEventSource<RE::InputEvent*>* eventSource) override;

    struct KeyCallbacks {
        std::map<KeyHandlerEvent, KeyCallback> down;
        std::map<KeyHandlerEvent, KeyCallback> up;
    };

    std::map<std::uint32_t, KeyCallbacks> _registeredCallbacks;
    std::map<KeyHandlerEvent, CallbackInfo> _handleMap;
    std::atomic<KeyHandlerEvent> _nextHandle = INVALID_KEY_HANDLER_EVENT + 1;
    std::shared_mutex _mutex;
};
