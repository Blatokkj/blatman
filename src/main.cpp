#include "commands.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char *argv[])
{
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
