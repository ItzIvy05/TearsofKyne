#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

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

#include <spdlog/sinks/basic_file_sink.h>

#include "Settings.h"

namespace logger = SKSE::log;
using namespace std::literals;
