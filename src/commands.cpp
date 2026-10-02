#include "commands.hpp"
#include <iostream>

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

void installCommand()
{
    std::cout << "Install ainda nao implementado.\n";
}

void removeCommand()
{
    std::cout << "Remove ainda nao implementado.\n";
}

void updateCommand()
{
    std::cout << "Update ainda nao implementado.\n";
}

void buildCommand()
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

void executeCommand(Command command)
{
    switch (command)
    {
        case Command::Help:
            printHelp();
            break;

        case Command::Install:
            installCommand();
            break;

        case Command::Remove:
            removeCommand();
            break;

        case Command::Update:
            updateCommand();
            break;

        case Command::Build:
            buildCommand();
            break;

        case Command::Unknown:
            std::cout << "Comando desconhecido.\n";
            printHelp();
            break;
    }
}