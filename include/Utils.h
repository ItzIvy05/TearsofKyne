#pragma once

namespace WaterskinUtils {
    void Initialize();
    void TryFill();
    [[nodiscard]] bool TryFillFromActivator(RE::TESObjectREFR* activatedRef, RE::TESObjectREFR* actionRef);
    void TryDrink();
    [[nodiscard]] bool IsNearWater();
    [[nodiscard]] bool IsRefillActivator(RE::TESObjectREFR* reference);
    [[nodiscard]] bool HasEmptyWaterskin();
    [[nodiscard]] bool HasFilledWaterskin();
    void GiveStartingWaterskin();
    void QueueStartingWaterskin();
    void CancelPendingStartingWaterskin();
    void OnInGameSessionReady();
    void OnObjectEquipped(RE::FormID baseObjectFormID, RE::TESObjectREFR* actorRef);
    void SetSystemEnabled(bool enabled);
    void SyncFillPower();
}