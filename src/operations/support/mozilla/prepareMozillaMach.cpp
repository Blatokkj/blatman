
#include "operations/installation/nativeInstall.hpp"
#include "operations/installation/base/baseInstall.hpp"
#include "operations/installation/details/installationHelpers.hpp"
#include "shell.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{

std::string makePrefixAssignment(const fs::path& prefix)
{
    std::string escaped;

    for (const char c : prefix.string())
    {
        if (c == ' ' || c == '\\' || c == '#')
            escaped += '\\';

        if (c == '$')
            escaped += '$';

        escaped += c;
    }

    return "prefix=" + escaped;
}

}

bool prepareMozillaMach(
    const fs::path& sourceDir,
    const fs::path& distributionDir,
    const fs::path& stage
)
{
    try
    {
        if (!installation_detail::validStage(stage))
            return false;

        const auto source =
            installation_detail::directory(sourceDir);

        if (!fs::is_regular_file(source / "mach"))
        {
            return installation_detail::fail(
                "executavel mach não encontrado."
            );
        }

        const auto& prefix =
            Blatman::Install::getInstallPaths().PREFIX;

        if (!prefix.is_absolute())
        {
            return installation_detail::fail(
                "o prefixo de instalação precisa ser absoluto."
            );
        }

        const std::string makeFlags =
            makePrefixAssignment(prefix);

        std::cout
            << "Mozilla Mach: preparando instalacao...\n";

        const auto installed = runShell(
            "cd " + quoteShell(source.string()) +
            " && env DESTDIR=" + quoteShell(stage.string()) +
            " MAKEFLAGS=" + quoteShell(makeFlags) +
            " ./mach install",
            true
        );

        if (!installed.success)
        {
            std::cerr << installed.output;

            return installation_detail::fail(
                "o alvo de instalação Mozilla Mach falhou. "
                "O projeto precisa oferecer um alvo install "
                "compatível com DESTDIR."
            );
        }

        return installation_detail::populated(stage);
    }
    catch (const std::exception& error)
    {
        return installation_detail::fail(error.what());
    }
}
