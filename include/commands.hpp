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

void printHelp();
bool installCommand(const std::string& argument);
bool removeCommand(const std::string& argument);
bool updateCommand(const std::string& argument);
bool buildCommand(const std::string& argument);
