#pragma once

#include <string>

std::string quoteShell(const std::string& value);

struct CommandResult
{
    bool success;
    std::string output;
};

CommandResult runShell(const std::string& command);