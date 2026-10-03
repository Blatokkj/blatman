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

void executeCommand(
    Command command,
    const std::string& argument
);

void printHelp();
void installCommand(const std::string& argument);
void removeCommand(const std::string& argument);
void updateCommand(const std::string& argument);
void buildCommand(const std::string& argument);