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

CommandResult runShell(const std::string& command)
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
        output += buffer;
        std::cout << buffer << std::flush;
    }

    const bool readFailed = std::ferror(pipe) != 0;
    const int status = pclose(pipe);

    return {status == 0 && !readFailed, output};
}