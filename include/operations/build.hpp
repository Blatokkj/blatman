#pragma once

#include "builderDetector.hpp"

#include <filesystem>

namespace fs = std::filesystem;

class Build
{
public:
    bool run(
        const fs::path& projectPath,
        BuildSystem system) const;
};