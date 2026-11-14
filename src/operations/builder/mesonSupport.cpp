#include "operations/builder/mesonSupport.hpp"
#include "operations/installation/details/installationHelpers.hpp"
#include "shell.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{

BuildResult failure(
    const std::string& message,
    const std::string& output = {}
)
{
    std::cerr << message << '\n';

    return {
        false,
        output + message + '\n',
        {}
    };
}

std::string sourceCommand(
    const fs::path& source,
    const std::string& command
)
{
    return "cd " + quoteShell(source.string()) +
           " && " + command;
}

}

BuildResult buildMesonProject(const fs::path& source)
{
    std::string output;

    try
    {
        if (!fs::is_directory(source))
            return failure("Diretório de origem inválido.");

        const auto root = fs::canonical(source);

        if (!fs::is_regular_file(root / "meson.build"))
            return failure(
                "Arquivo meson.build não encontrado no diretório de origem."
            );

        const auto buildDir = root / "build";
        const auto coreData =
            buildDir / "meson-private" / "coredata.dat";

        const bool reconfigure =
            fs::is_regular_file(coreData);

        // Configuração: sem prefixo ou diretórios de instalação.
        std::string setup =
            "meson setup " +
            std::string(reconfigure ? "--reconfigure " : "") +
            quoteShell(buildDir.string());

        const auto configured = runShell(
            sourceCommand(root, setup),
            true
        );

        output += configured.output;

        if (!configured.success)
            return failure(
                "Falha ao configurar o projeto Meson.",
                output
            );

        // Compilação: responsabilidade exclusiva do builder.
        const auto compiled = runShell(
            "meson compile -C " + quoteShell(buildDir.string()),
            true
        );

        output += compiled.output;

        if (!compiled.success)
            return failure(
                "Falha ao compilar o projeto Meson.",
                output
            );

        return {
            true,
            output,
            {}
        };
    }
    catch (const std::exception& error)
    {
        return failure(
            std::string("Falha no suporte Meson: ") +
            error.what(),
            output
        );
    }
}
