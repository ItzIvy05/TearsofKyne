#include "Serialization.h"

#include "Menu.h"
#include "Manager.h"

#include <atomic>

namespace {
    std::atomic_bool g_loadedSaveData = false;
}

namespace Serialization {
    void OnSave(SKSE::SerializationInterface* serialization)
    {
        if (!serialization->OpenRecord(Settings::SERIAL_RECORD, Settings::SERIAL_VER)) {
            logger::error("[Serialization] Failed to open save record.");
            return;
        }

        const auto* manager = WaterNeedManager::GetSingleton();
        const auto level = manager->GetLevel();
        const auto lastGameTime = manager->GetLastGameTime();
        const auto systemEnabled = manager->IsSystemEnabled();
        const auto storedBottleCounts = manager->GetStoredBottleCounts();
        const auto hoursWithoutDrink = manager->GetHoursWithoutDrink();

        serialization->WriteRecordData(level);
        serialization->WriteRecordData(lastGameTime);
        serialization->WriteRecordData(systemEnabled);
        for (const auto count : storedBottleCounts) {
            serialization->WriteRecordData(count);
        }
        serialization->WriteRecordData(hoursWithoutDrink);
    }

    void OnLoad(SKSE::SerializationInterface* serialization)
    {
        g_loadedSaveData.store(false);
        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;

        while (serialization->GetNextRecordInfo(type, version, length)) {
            if (type != Settings::SERIAL_RECORD) {
                continue;
            }

            float level = 0.0f;
            float lastGameTime = -1.0f;
            bool systemEnabled = true;
            std::array<std::int32_t, 4> storedBottleCounts{};
            float hoursWithoutDrink = 0.0f;

            serialization->ReadRecordData(level);
            serialization->ReadRecordData(lastGameTime);

            if (version >= 2) {
                serialization->ReadRecordData(systemEnabled);
                for (auto& count : storedBottleCounts) {
                    serialization->ReadRecordData(count);
                }
            }

            if (version >= 3) {
                serialization->ReadRecordData(hoursWithoutDrink);
            }

            g_loadedSaveData.store(true);

            WaterNeedManager::GetSingleton()->ForceSet(level);
            WaterNeedManager::GetSingleton()->SetLastGameTime(lastGameTime);
            WaterNeedManager::GetSingleton()->SetSystemEnabled(systemEnabled);
            WaterNeedManager::GetSingleton()->SetStoredBottleCounts(storedBottleCounts);
            WaterNeedManager::GetSingleton()->SetHoursWithoutDrink(hoursWithoutDrink);
        }
    }

    void OnRevert(SKSE::SerializationInterface*)
    {
        g_loadedSaveData.store(false);
        WaterNeedManager::GetSingleton()->ForceSet(0.0f);
        WaterNeedManager::GetSingleton()->SetLastGameTime(-1.0f);
        WaterNeedManager::GetSingleton()->SetSystemEnabled(true);
        WaterNeedManager::GetSingleton()->ClearStoredBottleCounts();
        WaterNeedManager::GetSingleton()->SetHoursWithoutDrink(0.0f);
        TearsWidget::EndGameSession(false);
        TearsWidget::Refresh();
    }

    void ResetLoadState()
    {
        g_loadedSaveData.store(false);
    }

    bool HasLoadedSaveData()
    {
        return g_loadedSaveData.load();
    }
}
