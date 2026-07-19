#include "BuildDetector.hpp"
#include <algorithm>

BuildSystem BuildDetector::detect(const fs::path& repoPath) const {
    if (fs::exists(repoPath / "CMakeLists.txt")) {
        return BuildSystem::CMake;
    } else if (fs::exists(repoPath / "meson.build")) {
        return BuildSystem::Meson;
    } else if (fs::exists(repoPath / "Cargo.toml")) {
        return BuildSystem::Cargo;
    } else if (fs::exists(repoPath / "go.mod")) {
        return BuildSystem::Go;
    } else if (fs::exists(repoPath / "Makefile")) {
        return BuildSystem::Make;
    } else if (fs::exists(repoPath / "package.json")) {
        return BuildSystem::Npm;
    } else if (fs::exists(repoPath / "pom.xml")) {
        return BuildSystem::Maven;
    } else if (fs::exists(repoPath / "build.gradle") || fs::exists(repoPath / "build.gradle.kts")) {
        return BuildSystem::Gradle;
    } else if (fs::exists(repoPath / "configure") || fs::exists(repoPath / "configure.ac")) {
        return BuildSystem::Autotools;
    }
    return BuildSystem::Unknown;
}