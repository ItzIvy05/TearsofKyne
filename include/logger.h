#pragma once

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>

static bool ReadLoggingEnabledFromINI() {
    wchar_t modulePath[MAX_PATH]{};
    const HMODULE module = GetModuleHandleW(L"Tears of Kyne.dll");
    if (!module) return false;
    const DWORD length = GetModuleFileNameW(module, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return false;
    const std::filesystem::path iniPath = std::filesystem::path(modulePath).parent_path() / L"TearsOfKyne.ini";

    std::error_code ec;
    if (!std::filesystem::exists(iniPath, ec)) {
        return false;
    }
    std::ifstream in(iniPath);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        const size_t a = key.find_first_not_of(" \t");
        const size_t b = key.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        key = key.substr(a, b - a + 1);
        if (key == "bEnableLogging") {
            const size_t va = value.find_first_not_of(" \t\r\n");
            if (va == std::string::npos) return false;
            return value[va] == '1';
        }
    }
    return false;
}

static void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));

    const bool loggingEnabled = ReadLoggingEnabledFromINI();
    if (!loggingEnabled) {
        spdlog::set_level(spdlog::level::off);
        return;
    }
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
#endif
    logger::info("Name of the plugin is {}.", pluginName);
    logger::info("Version of the plugin is {}.", SKSE::PluginDeclaration::GetSingleton()->GetVersion());
}