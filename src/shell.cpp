#include "shell.hpp"

std::string quoteShell(const std::string& value)
{
    std::string result = "'";

    for (char character : value)
    {
        if (character == '\'')
            result += "'\\''";
        else
            result += character;
    }

    result += "'";
    return result;
}