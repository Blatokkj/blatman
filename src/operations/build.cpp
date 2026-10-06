#include "operations/build.hpp"
#include "shell.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

bool Build::run(
    const fs::path& projectPath,
    BuildSystem system) const
{
    const fs::path sourcePath = fs::absolute(projectPath);

    std::vector<std::string> commands;

    switch (system)
    {
       case BuildSystem::CMake:
           commands = {
                "cmake -S . -B build",
                "cmake --build build"
            };
            break;

        case BuildSystem::Make:
            commands = {
                "make"
            };
            break;

        case BuildSystem::Meson:
            commands = {
                "meson setup --reconfigure build",
                "meson compile -C build"
            };
            break;

        case BuildSystem::Cargo:
            commands = {
                "cargo build"
            };
            break;

        case BuildSystem::Npm:
            commands = {
                "npm run build"
            };
            break;

        case BuildSystem::Maven:
            if (fs::is_regular_file(sourcePath / "mvnw"))
                commands = {"./mvnw package"};
            else
                commands = {"mvn package"};
            break;

        case BuildSystem::Gradle:
            if (fs::is_regular_file(sourcePath / "gradlew"))
                commands = {"./gradlew build"};
            else
                commands = {"gradle build"};
            break;

        case BuildSystem::Autotools:
            if (!fs::is_regular_file(sourcePath / "configure"))
                commands.push_back("autoreconf -fi");

            commands.push_back("./configure");
            commands.push_back("make");
            break;

        case BuildSystem::Go:
            commands = {
                "go build ./..."
            };
            break;

        case BuildSystem::Unknown:
            std::cerr << "Nenhum sistema de build reconhecido.\n";
            return false;

        default:
            std::cerr << "Sistema de build invalido.\n";
            return false;
    }

    const std::string directoryCommand =
        "cd " + quoteShell(sourcePath.string()) + " && ";

    for (const auto& command : commands)
    {
        std::cout << "Executando: " << command << '\n';

        const std::string fullCommand = directoryCommand + command;

        if (std::system(fullCommand.c_str()) != 0)
        {
            std::cerr << "Falha ao executar: " << command << '\n';
            return false;
        }
    }

    return true;
}