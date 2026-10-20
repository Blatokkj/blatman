#pragma once

#include <string>
#include <cstddef>

std::string quoteShell(const std::string& value);

struct CommandResult
{
    bool success;
    std::string output;
};

CommandResult runShell(const std::string& command, bool echo = true,
                       std::size_t captureLimit = 0);