#include "Events.h"

#include "Menu.h"
#include "Utils.h"

namespace Events {
    namespace {
        class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuWatcher* GetSingleton()
            {
                static MenuWatcher instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const std::string_view menuName = event->menuName.c_str();
                

                TearsWidget::NotifyMenuEvent(event->menuName.c_str(), event->opening);
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class ActivateWatcher final : public RE::BSTEventSink<RE::TESActivateEvent> {
        public:
            static ActivateWatcher* GetSingleton()
            {
                static ActivateWatcher instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESActivateEvent* event,
                RE::BSTEventSource<RE::TESActivateEvent>*) override
            {
                if (!event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (WaterskinUtils::TryFillFromActivator(event->objectActivated.get(), event->actionRef.get())) {
                    return RE::BSEventNotifyControl::kStop;
                }

                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class FillPowerWatcher final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
        public:
            static FillPowerWatcher* GetSingleton()
            {
                static FillPowerWatcher instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const SKSE::ModCallbackEvent* event,
                RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
            {
                if (event && event->eventName == "TOK_FillWater") {
                    if (auto* taskInterface = SKSE::GetTaskInterface()) {
                        taskInterface->AddTask([] { WaterskinUtils::TryFill(); });
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class EquipWatcher final : public RE::BSTEventSink<RE::TESEquipEvent> {
        public:
            static EquipWatcher* GetSingleton()
            {
                static EquipWatcher instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESEquipEvent* event,
                RE::BSTEventSource<RE::TESEquipEvent>*) override
            {
                if (!event || !event->equipped) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                WaterskinUtils::OnObjectEquipped(event->baseObject, event->actor.get());
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void RegisterAll()
    {
        bool registeredAnything = false;

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
            logger::info("[Events] Registered MenuOpenCloseEvent sink.");
            TearsWidget::RefreshSuppression();
            registeredAnything = true;
        } else {
            logger::warn("[Events] Could not register MenuOpenCloseEvent sink.");
        }

        if (auto* modCallbackSource = SKSE::GetModCallbackEventSource()) {
            modCallbackSource->AddEventSink(FillPowerWatcher::GetSingleton());
            logger::info("[Events] Registered ModCallbackEvent sink.");
            registeredAnything = true;
        } else {
            logger::warn("[Events] Could not register ModCallbackEvent sink.");
        }

        if (auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton()) {
            if (auto* activateSource = eventSourceHolder->GetEventSource<RE::TESActivateEvent>()) {
                activateSource->AddEventSink(ActivateWatcher::GetSingleton());
                logger::info("[Events] Registered TESActivateEvent sink.");
                registeredAnything = true;
            } else {
                logger::warn("[Events] Could not acquire TESActivateEvent source.");
            }

            if (auto* equipSource = eventSourceHolder->GetEventSource<RE::TESEquipEvent>()) {
                equipSource->AddEventSink(EquipWatcher::GetSingleton());
                logger::info("[Events] Registered TESEquipEvent sink.");
                registeredAnything = true;
            } else {
                logger::warn("[Events] Could not acquire TESEquipEvent source.");
            }
        } else {
            logger::warn("[Events] ScriptEventSourceHolder unavailable.");
        }

        if (!registeredAnything) {
            logger::warn("[Events] No gameplay event sinks were registered.");
        }
    }
}
