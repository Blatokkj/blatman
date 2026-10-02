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

void executeCommand(Command command);

void printHelp();
void installCommand();
void removeCommand();
void updateCommand();
void buildCommand();