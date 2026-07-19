#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

enum class BuildSystem {
    CMake,
    Make,
    Meson,
    Cargo,
    Npm,
    Maven,
    Gradle,
    Autotools,
    Go,
    Unknown
};

class BuildDetector {
public:
    BuildSystem detect(const fs::path& repoPath) const;
};