#include "Events.h"

#include "Menu.h"
#include "SurvivalWidgets.h"
#include "Utils.h"

namespace Events {
    namespace {
        class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>, public REX::Singleton<MenuWatcher> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                TearsWidget::NotifyMenuEvent(event->menuName.c_str(), event->opening);
                SurvivalWidgets::NotifyMenuEvent(event->menuName.c_str(), event->opening);
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class ActivateWatcher final : public RE::BSTEventSink<RE::TESActivateEvent>, public REX::Singleton<ActivateWatcher> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*) override {
                if (!event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (WaterskinUtils::TryFillFromActivator(event->objectActivated.get(), event->actionRef.get())) {
                    return RE::BSEventNotifyControl::kStop;
                }

                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class FillPowerWatcher final : public RE::BSTEventSink<SKSE::ModCallbackEvent>, public REX::Singleton<FillPowerWatcher> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
                if (event && event->eventName == "TOK_FillWater") {
                    if (auto* taskInterface = SKSE::GetTaskInterface()) {
                        taskInterface->AddTask([] { WaterskinUtils::TryFill(); });
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class EquipWatcher final : public RE::BSTEventSink<RE::TESEquipEvent>, public REX::Singleton<EquipWatcher> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override {
                if (!event || !event->equipped) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                WaterskinUtils::OnObjectEquipped(event->baseObject, event->actor.get());
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class SurvivalNeedWatcher final : public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>, public REX::Singleton<SurvivalNeedWatcher> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* event, RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*) override {
                if (!event || !SurvivalWidgets::IsAvailable()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (event->target.get() != RE::PlayerCharacter::GetSingleton()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (_pending.exchange(true)) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([] {
                        _pending.store(false);
                        SurvivalWidgets::Notify();
                    });
                } else {
                    _pending.store(false);
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            static inline std::atomic<bool> _pending{false};
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
            eventSourceHolder->AddEventSink<RE::TESActivateEvent>(ActivateWatcher::GetSingleton());
            eventSourceHolder->AddEventSink<RE::TESEquipEvent>(EquipWatcher::GetSingleton());
            logger::info("[Events] Registered TESActivateEvent and TESEquipEvent sinks.");
            registeredAnything = true;

            if (SurvivalWidgets::IsAvailable()) {
                eventSourceHolder->AddEventSink<RE::TESActiveEffectApplyRemoveEvent>(SurvivalNeedWatcher::GetSingleton());
                logger::info("[Events] Registered TESActiveEffectApplyRemoveEvent sink.");
            }
        } else {
            logger::warn("[Events] ScriptEventSourceHolder unavailable.");
        }

        if (!registeredAnything) {
            logger::warn("[Events] No gameplay event sinks were registered.");
        }
    }
}
