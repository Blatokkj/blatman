#include "builderDetector.hpp"

BuildSystem BuildDetector::detect(const fs::path& repoPath) const
{
    if (fs::exists(repoPath / "CMakeLists.txt"))
        return BuildSystem::CMake;

    if (fs::exists(repoPath / "meson.build"))
        return BuildSystem::Meson;

    if (fs::exists(repoPath / "Cargo.toml"))
        return BuildSystem::Cargo;

    if (fs::exists(repoPath / "go.mod"))
        return BuildSystem::Go;

    if (fs::exists(repoPath / "Makefile"))
        return BuildSystem::Make;

    if (fs::exists(repoPath / "package.json"))
        return BuildSystem::Npm;

    if (fs::exists(repoPath / "pom.xml"))
        return BuildSystem::Maven;

    if (fs::exists(repoPath / "build.gradle") ||
        fs::exists(repoPath / "build.gradle.kts"))
        return BuildSystem::Gradle;

    if (fs::exists(repoPath / "configure") ||
        fs::exists(repoPath / "configure.ac"))
        return BuildSystem::Autotools;

    return BuildSystem::Unknown;
}