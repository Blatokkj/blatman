#pragma once

#include <filesystem>

namespace fs = std::filesystem;

enum class BuildSystem
{
    CMake, Make, Meson, Cargo, Npm, Maven, Gradle, Autotools, Go, Unknown
};

const char* buildSystemName(BuildSystem system);

class BuildDetector
{
public:
    BuildSystem detect(const fs::path& repoPath, bool useOverride = true) const;
};
