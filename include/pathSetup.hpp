#pragma once

#include <filesystem>

namespace Blatman::Install {
    std::filesystem::path getHomePath();
    bool setupPath();
}