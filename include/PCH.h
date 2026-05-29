#pragma once

// RE/Skyrim.h MUST be first — CommonLib-NG's REX layer errors if <Windows.h>
// is detected before it. CommonLib handles Windows headers internally.
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

// Standard library
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <fstream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// spdlog
#include <spdlog/sinks/basic_file_sink.h>

// PrismaUI — included after CommonLib so its <Windows.h> doesn't cause the REX error
#include "PrismaUI_API.h"

// Project-wide headers — included here so every .cpp gets them via PCH
// without needing explicit includes in individual .h files
#include "Settings.h"

namespace logger = SKSE::log;
using namespace std::literals;
