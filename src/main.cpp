#include "packageManager.hpp"
#include "commands.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        executeCommand(Command::Help);
        return 0;
    }

    Command command = parseCommand(argv[1]);

    executeCommand(command);

    return 0;
}