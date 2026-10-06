#include "commands.hpp"
#include "operations/install.hpp"
#include "builderDetector.hpp"
#include "operations/build.hpp"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void printHelp()
{
    std::cout << R"(
Blatman - Universal Package Manager

Uso:
  blatman <comando> [opções] [argumentos]

Comandos:
  install <pacote>       Instala um pacote
  remove <pacote>        Remove um pacote
  update                 Atualiza os pacotes
  update <pacote>        Atualiza um pacote específico
  build <projeto>        Detecta e executa o sistema de build
  help                   Mostra esta ajuda

Opções:
  -h, --help             Mostra esta ajuda
  -v, --version          Mostra a versão do Blatman

Exemplos:
  blatman install firefox
  blatman remove firefox
  blatman update
  blatman build .
)";
}

void installCommand(const std::string& argument)
{
    if (argument.empty())
    {
        std::cerr << "Erro: informe a URL do repositório.\n";
        return;
    }
    
    try
    {
        fs::path cacheRoot = fs::current_path() / "blatman-cache";


    Install installer;

    std::cout << "Clonando: " << argument << '\n';

    const auto repositoryPath = installer.clone(argument, cacheRoot);

    if (!repositoryPath)
    {
        std::cerr << "Operação de instalação interrompida.\n";
        return;
    }

    std::cout << "Repositório clonado com sucesso!\n";
    std::cout << "Procurando sistema de build...\n";

    BuildDetector detector;
    BuildSystem system = detector.detect(*repositoryPath);

    const char* name = "";

    switch (system)
    {
        case BuildSystem::CMake:
            name = "CMake";
            break;
        case BuildSystem::Make:
            name = "Make";
            break;
        case BuildSystem::Meson:
            name = "Meson";
            break;
        case BuildSystem::Cargo:
            name = "Cargo";
            break;
        case BuildSystem::Npm:
            name = "Npm";
            break;
        case BuildSystem::Maven:
            name = "Maven";
            break;
        case BuildSystem::Gradle:
            name = "Gradle";
            break;
        case BuildSystem::Autotools:
            name = "Autotools";
            break;
        case BuildSystem::Go:
            name = "Go";
            break;
        case BuildSystem::Unknown:
            std::cout << "Nenhum sistema de build reconhecido.\n";
            return;
    }
    
            std::cout << "Sistema detectado: " << name << '\n';
    }

    catch (const fs::filesystem_error& error)
    {
        std::cerr << "Erro de filesystem durante a instalacao: "
                  << error.what() << '\n';
    }
}

void removeCommand(const std::string& argument)
{
    std::cout << "Remove ainda nao implementado.\n";
}

void updateCommand(const std::string& argument)
{
    std::cout << "Update ainda nao implementado.\n";
}

void buildCommand(const std::string& argument)
{
    if (argument.empty())
    {
        std::cerr << "Erro: informe o diretorio do projeto.\n";
        return;
    }

    const fs::path projectPath = argument;

    try
    {
        if (!fs::is_directory(projectPath))
        {
            std::cerr << "Erro: informe um diretorio existente.\n";
            return;
        }

        BuildDetector detector;
        const BuildSystem system = detector.detect(projectPath);

        Build builder;

        if (!builder.run(projectPath, system))
            return;

        std::cout << "Build concluido com sucesso!\n";
    }
    catch (const fs::filesystem_error& error)
    {
        std::cerr << "Erro de filesystem: " << error.what() << '\n';
    }
}

Command parseCommand(const std::string& command)
{
    if (command == "help" ||
        command == "--help" ||
        command == "-h")
        return Command::Help;

    if (command == "install" ||
        command == "-S" ||
        command == "-i")
        return Command::Install;

    if (command == "remove" ||
        command == "-R" ||
        command == "-r")
        return Command::Remove;

    if (command == "update" ||
        command == "-U" ||
        command == "-u")
        return Command::Update;

    if (command == "build" ||
        command == "-B" ||
        command == "-b")
        return Command::Build;

    return Command::Unknown;
}

void executeCommand(
    Command command,
    const std::string& argument)
{
    switch (command)
    {
        case Command::Help:
            printHelp();
            break;

        case Command::Install:
            installCommand(argument);
            break;

        case Command::Remove:
            removeCommand(argument);
            break;

        case Command::Update:
            updateCommand(argument);
            break;

        case Command::Build:
            buildCommand(argument);
            break;

        case Command::Unknown:
            std::cout << "Comando desconhecido.\n";
            printHelp();
            break;
    }
}
