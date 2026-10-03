#include "commands.hpp"
#include "functions/install.hpp"

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
        std::cout << "Erro: informe a URL do repositorio.\n";
        return;
    }

    fs::path destination =
        fs::current_path() / "blatman-cache";

    Install installer;

    std::cout << "Clonando: " << argument << '\n';
    std::cout << "Destino: " << destination << '\n';

    if (installer.clone(argument, destination))
    {
        std::cout << "Repositorio clonado com sucesso!\n";
    }
    else
    {
        std::cout << "Falha ao clonar o repositorio.\n";
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
    std::cout << "Build ainda nao implementado.\n";
}

Command parseCommand(const std::string& command)
{
    if (command == "help" ||
        command == "--help" ||
        command == "-h")
        return Command::Help;

    if (command == "install")
        return Command::Install;

    if (command == "remove")
        return Command::Remove;

    if (command == "update")
        return Command::Update;

    if (command == "build")
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