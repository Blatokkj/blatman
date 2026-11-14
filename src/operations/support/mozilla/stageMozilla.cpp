#include "operations/installation/details/mozilla.hpp"

#include "shell.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace mozilla_detail
{
namespace
{
    std::string escapeMakeValue(const std::string& value)
    {
        std::string escaped;

        for (const char c : value)
        {
            if (c == '\n' || c == '\r')
            {
                throw std::runtime_error(
                    "O prefixo contém caracteres incompatíveis com MAKEFLAGS."
                );
            }

            switch (c)
            {
                case ' ':
                case '\t':
                case '#':
                    escaped += '\\';
                    escaped += c;
                    break;

                case '\\':
                    escaped += "\\\\";
                    break;

                case '$':
                    escaped += "$$$$";
                    break;

                default:
                    escaped += c;
                    break;
            }
        }

        return escaped;
    }
}

bool stageMozilla(
    const Environment& environment,
    const fs::path& stage,
    const fs::path& prefix
)
{
    if (!prefix.is_absolute() ||
        prefix == prefix.root_path() ||
        prefix != prefix.lexically_normal())
    {
        return installation_detail::fail(
            "O prefixo de instalação é inválido."
        );
    }

    const char* currentFlags = std::getenv("MAKEFLAGS");

    std::string makeFlags =
        currentFlags ? currentFlags : "";

    if (!makeFlags.empty())
        makeFlags += ' ';

    makeFlags += "prefix=" + escapeMakeValue(prefix.string());
    makeFlags += " PREFIX=" + escapeMakeValue(prefix.string());

    std::cout << "Mozilla Mach: executando instalação no staging...\n";

    const auto installed = runShell(
        "cd " + quoteShell(environment.source.string()) +
        " && env" +
        " DESTDIR=" + quoteShell(stage.string()) +
        " prefix=" + quoteShell(prefix.string()) +
        " PREFIX=" + quoteShell(prefix.string()) +
        " MAKEFLAGS=" + quoteShell(makeFlags) +
        " ./mach install",
        true
    );

    if (!installed.success)
    {
        if (!installed.output.empty())
            std::cerr << installed.output;

        return installation_detail::fail(
            "O alvo de instalação Mozilla Mach falhou. "
            "O projeto precisa oferecer um alvo install compatível com DESTDIR."
        );
    }

    return true;
}

}