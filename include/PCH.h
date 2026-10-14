#pragma once

#include "RE/Skyrim.h"
#include "REX/REX/Singleton.h"
#include "SKSE/SKSE.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
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

#include <ClibUtil/rng.hpp>
#include <ClibUtil/string.hpp>

#include "Settings.h"

namespace logger = SKSE::log;
using namespace std::literals;
