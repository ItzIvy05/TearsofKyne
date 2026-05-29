#pragma once

namespace Serialization {
    void OnSave(SKSE::SerializationInterface* serialization);
    void OnLoad(SKSE::SerializationInterface* serialization);
    void OnRevert(SKSE::SerializationInterface* serialization);

    void ResetLoadState();
    [[nodiscard]] bool HasLoadedSaveData();
}
