#pragma once

#include <string>

namespace Localization {
    void Load();
    const std::string& Get(const char* key);
}