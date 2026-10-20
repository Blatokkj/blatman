#include "shell.hpp"

#include <cstdio>
#include <iostream>

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

CommandResult runShell(const std::string& command, bool echo, std::size_t captureLimit)
{
    // Padroniza o idioma dos diagnosticos e captura stdout + stderr.
    const std::string wrappedCommand =
        "(export LC_ALL=C; " + command + ") 2>&1";

    std::cout.flush();

    FILE* pipe = popen(wrappedCommand.c_str(), "r");

    if (pipe == nullptr)
    {
        std::cerr << "Nao foi possivel iniciar o comando.\n";
        return {false, ""};
    }

    std::string output;
    char buffer[4096];

    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        // Only bound Java build logs (other callers pass captureLimit=0).
        const std::size_t size = std::char_traits<char>::length(buffer);
        if (captureLimit != 0 && size >= captureLimit)
            output.assign(buffer + size - captureLimit, captureLimit);
        else
        {
            if (captureLimit != 0 && output.size() + size > captureLimit)
                output.erase(0, output.size() + size - captureLimit);
            output.append(buffer, size);
        }
        if (echo) std::cout << buffer << std::flush;
    }

    const bool readFailed = std::ferror(pipe) != 0;
    const int status = pclose(pipe);

    return {status == 0 && !readFailed, output};
}