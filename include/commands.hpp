#pragma once
#include <string>

enum class Command
{
    Help,
    Install,
    Remove,
    Update,
    Build,
    Unknown
};

Command parseCommand(const std::string& command);

bool executeCommand(
    Command command,
    const std::string& argument
);

