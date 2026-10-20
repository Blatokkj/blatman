#include "commands.hpp"
#include "installTransaction.hpp"
#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc == 3 && std::string(argv[1]) == "--publish-stage")
    {
        if (::geteuid() != 0)
        {
            std::cerr << "Publicacao no sistema requer privilegios.\n";
            return EXIT_FAILURE;
        }
        return publishTree(argv[2], "/") ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (::geteuid() == 0 && argc > 1 &&
        (parseCommand(argv[1]) == Command::Install || parseCommand(argv[1]) == Command::Build))
    {
        std::cerr << "Execute sem sudo: somente a publicacao final pede privilegios.\n";
        return EXIT_FAILURE;
    }
    if (argc > 3)
    {
        std::cerr << "Erro: argumentos em excesso. Consulte blatman help.\n";
        return EXIT_FAILURE;
    }

    const Command command = argc < 2 ? Command::Help : parseCommand(argv[1]);
    const std::string argument = argc >= 3 ? argv[2] : "";

    try
    {
        return executeCommand(command, argument) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Erro: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
