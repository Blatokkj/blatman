#include "builderDetector.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

const char* buildSystemName(BuildSystem system)
{
    switch (system)
    {
        case BuildSystem::CMake: return "CMake";
        case BuildSystem::Make: return "Make";
        case BuildSystem::Meson: return "Meson";
        case BuildSystem::Cargo: return "Cargo";
        case BuildSystem::Npm: return "Npm";
        case BuildSystem::Maven: return "Maven";
        case BuildSystem::Gradle: return "Gradle";
        case BuildSystem::Autotools: return "Autotools";
        case BuildSystem::Go: return "Go";
        case BuildSystem::Unknown: return "Unknown";
    }
    return "Unknown";
}

BuildSystem BuildDetector::detect(const fs::path& repoPath, bool useOverride) const
{
    const auto has = [&](const char* file) { return fs::is_regular_file(repoPath / file); };
    std::vector<BuildSystem> systems;
    if (has("CMakeLists.txt")) systems.push_back(BuildSystem::CMake);
    if (has("meson.build")) systems.push_back(BuildSystem::Meson);
    if (has("Cargo.toml")) systems.push_back(BuildSystem::Cargo);
    if (has("go.mod") || has("go.work")) systems.push_back(BuildSystem::Go);
    if (has("configure") || has("configure.ac") || has("configure.in"))
        systems.push_back(BuildSystem::Autotools);
    if (has("package.json")) systems.push_back(BuildSystem::Npm);

    const bool maven = has("pom.xml");
    const bool gradle = has("build.gradle") || has("build.gradle.kts") ||
                        has("settings.gradle") || has("settings.gradle.kts");
    const bool mavenWrapper = has("mvnw");
    const bool gradleWrapper = has("gradlew");
    // Entre dois manifests Java, o unico wrapper indica o fluxo distribuido pelo projeto.
    if (maven && (!gradle || mavenWrapper == gradleWrapper || mavenWrapper))
        systems.push_back(BuildSystem::Maven);
    if (gradle && (!maven || mavenWrapper == gradleWrapper || gradleWrapper))
        systems.push_back(BuildSystem::Gradle);

    const bool make = has("GNUmakefile") || has("makefile") || has("Makefile");
    if (useOverride)
    {
        if (const char* override = std::getenv("BLATMAN_BUILD_SYSTEM"); override && *override)
        {
            std::string wanted(override);
            std::transform(wanted.begin(), wanted.end(), wanted.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            // A escolha explicita tambem pode selecionar um Makefile auxiliar.
            auto candidates = systems;
            if (make) candidates.push_back(BuildSystem::Make);
            if (maven) candidates.push_back(BuildSystem::Maven);
            if (gradle) candidates.push_back(BuildSystem::Gradle);
            for (const auto candidate : candidates)
            {
                std::string name(buildSystemName(candidate));
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (wanted == name) return candidate;
            }
            std::cerr << "BLATMAN_BUILD_SYSTEM nao corresponde a um manifesto presente: "
                      << override << '\n';
            return BuildSystem::Unknown;
        }
    }
    if (systems.size() == 1) return systems.front();
    if (systems.empty()) return make ? BuildSystem::Make : BuildSystem::Unknown;
    std::cerr << "Mais de um sistema de build foi encontrado:";
    for (const auto system : systems) std::cerr << ' ' << buildSystemName(system);
    std::cerr << ". Selecione um usando BLATMAN_BUILD_SYSTEM.\n";
    return BuildSystem::Unknown;
}
