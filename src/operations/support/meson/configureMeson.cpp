
#include "shell.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace meson_detail
{

namespace
{

std::string relativeDirectory(
    const fs::path& destination,
    const fs::path& prefix
)
{
    const auto absolute =
        fs::absolute(destination).lexically_normal();

    if (!installation_detail::within(absolute, prefix) ||
        absolute == prefix)
    {
        throw std::runtime_error(
            "diretório fora do prefixo: " +
            destination.string()
        );
    }

    const auto relative =
        absolute.lexically_relative(prefix);

    if (relative.empty() || relative.is_absolute())
    {
        throw std::runtime_error(
            "não foi possível calcular o diretório relativo: " +
            destination.string()
        );
    }

    for (const auto& part : relative)
    {
        if (part == "..")
        {
            throw std::runtime_error(
                "diretório relativo escapa do prefixo: " +
                destination.string()
            );
        }
    }

    return relative.generic_string();
}

}

bool configureAndCompile(
    const fs::path& build,
    const fs::path& prefix,
    const fs::path& bin,
    const fs::path& lib,
    const fs::path& include,
    const fs::path& share
)
{
    const std::vector<std::pair<std::string, std::string>> options{
        {"--prefix", prefix.string()},
        {"--bindir", relativeDirectory(bin, prefix)},
        {"--sbindir", relativeDirectory(bin, prefix)},
        {"--libdir", relativeDirectory(lib, prefix)},
        {"--includedir", relativeDirectory(include, prefix)},
        {"--datadir", relativeDirectory(share, prefix)},
        {"--mandir", relativeDirectory(share / "man", prefix)},
        {"--infodir", relativeDirectory(share / "info", prefix)},
        {"--localedir", relativeDirectory(share / "locale", prefix)},
        {"--sysconfdir", "etc"},
        {"--localstatedir", "var"},
        {"--sharedstatedir", "var/lib"},
        {"--libexecdir", "libexec"}
    };

    std::ostringstream configureCommand;

    configureCommand
        << "meson configure "
        << quoteShell(build.string());

    for (const auto& [option, value] : options)
    {
        configureCommand
            << " "
            << quoteShell(option + "=" + value);
    }

    std::cout << "Meson: configurando destinos do Blatman...\n";

    const auto configured = runShell(
        configureCommand.str(),
        true
    );

    if (!configured.success)
    {
        throw std::runtime_error(
            "falha ao reconfigurar o projeto Meson.\n" +
            configured.output
        );
    }

    const auto compiled = runShell(
        "meson compile -C " + quoteShell(build.string()),
        true
    );

    if (!compiled.success)
    {
        throw std::runtime_error(
            "falha ao atualizar a compilação Meson.\n" +
            compiled.output
        );
    }

    return true;
}

}
