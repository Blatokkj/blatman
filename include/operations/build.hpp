#pragma once

#include "builderDetector.hpp"
#include "shell.hpp"

#include <filesystem>

namespace fs = std::filesystem;

class Build
{
public:
    CommandResult run(
        const fs::path& projectPath,
        BuildSystem system) const;
};