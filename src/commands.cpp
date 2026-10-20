#include "commands.hpp"
#include "operations/install.hpp"
#include "operations/build.hpp"
#include "operations/help.hpp"
#include "operations/remove.hpp"
#include "operations/update.hpp"
#include <iostream>
Command parseCommand(const std::string &command)
{
    if (command == "help" || command == "--help" || command == "-h")
        return Command::Help;

    if (command == "install" || command == "-S" || command == "-i")
        return Command::Install;

    if (command == "remove" || command == "-R" || command == "-r")
        return Command::Remove;

    if (command == "update" || command == "-U" || command == "-u")
        return Command::Update;

    if (command == "build" || command == "-B" || command == "-b")
        return Command::Build;

    return Command::Unknown;
}

bool executeCommand(Command command, const std::string &argument)
{
    switch (command)
    {
    case Command::Help:
        return helpCommand(argument);
    case Command::Install:
        return Install{}.execute(argument);
    case Command::Build:
        return Build{}.execute(argument);
    case Command::Remove:
        return removeCommand(argument);
    case Command::Update:
        return updateCommand(argument);
    case Command::Unknown:
        std::cerr << "Comando desconhecido.\n";
        return false;
    }
    return false;
}
