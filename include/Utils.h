#pragma once

namespace WaterskinUtils {
    void Initialize();
    void TryFill();
    [[nodiscard]] bool TryFillFromActivator(RE::TESObjectREFR* activatedRef, RE::TESObjectREFR* actionRef);
    [[nodiscard]] bool IsNearWater();
    void QueueStartingWaterskin();
    void CancelPendingStartingWaterskin();
    void OnInGameSessionReady();
    void OnObjectEquipped(RE::FormID baseObjectFormID, RE::TESObjectREFR* actorRef);
    void SetSystemEnabled(bool enabled);
    void SyncFillPower();
    void SyncDirtyWater();
}