#include "operations/installation/details/mozilla.hpp"

#include "shell.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <iostream>

namespace mozilla_detail
{

bool inspectInstallMozilla(const Environment& environment)
{
    if (!fs::is_regular_file(environment.object / "Makefile"))
    {
        return installation_detail::fail(
            "O diretório de objetos não contém um Makefile configurado."
        );
    }

    const auto inspected = runShell(
        "cd " + quoteShell(environment.source.string()) +
        " && ./mach help install",
        false
    );

    if (!inspected.success)
    {
        if (!inspected.output.empty())
            std::cerr << inspected.output;

        return installation_detail::fail(
            "O projeto não disponibiliza o comando mach install."
        );
    }

    return true;
}

}