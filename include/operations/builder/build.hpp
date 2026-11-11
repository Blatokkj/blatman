#pragma once

#include "builderDetector.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct BuildResult
{
    bool success;
    std::string output;
    std::vector<std::filesystem::path> executables;
    std::filesystem::path distribution;
};

class Build
{
public:
    bool execute(const std::string& argument) const;

    BuildResult run(const std::filesystem::path& projectPath,
                    BuildSystem system,
                    const std::filesystem::path& executableDirectory = {}) const;
};