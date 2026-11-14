#include "shell.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace meson_detail
{

    bool installMeson(
        const fs::path& build,
        const fs::path& stage
    )
    {
        std::cout << "Meson: instalando no staging...\n";

        const auto installation = runShell(
            "env DESTDIR=" + quoteShell(stage.string()) +
            " meson install --no-rebuild -C " +
            quoteShell(build.string()),
            true
        );

        if (!installation.success)
        {
            throw std::runtime_error(
                "falha durante a instalação Meson.\n" +
                installation.output
            );
        }

    return true;

    }

}