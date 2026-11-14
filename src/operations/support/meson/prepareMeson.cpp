#include "operations/installation/nativeInstall.hpp"
#include "operations/installation/base/baseInstall.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace meson_detail
{
    std::vector<std::string> inspectMeson(
        const fs::path& build
    );

    std::size_t validateDestinations(
        const std::vector<std::string>& destinations,
        const fs::path& prefix
    );

    bool validateStage(
        const fs::path& stage,
        const fs::path& prefix
    );

    bool configureAndCompile(
        const fs::path& build,
        const fs::path& prefix,
        const fs::path& bin,
        const fs::path& lib,
        const fs::path& include,
        const fs::path& share
    );

    bool installMeson(
        const fs::path& build,
        const fs::path& stage
    );
}

bool prepareMeson(
    const fs::path& buildDir,
    const fs::path& stage
)
{
    try
    {
        using namespace installation_detail;

        // 1. Validar o staging.
        if (!validStage(stage))
            return false;

        // 2. Resolver o diretório de build.
        const auto build = directory(buildDir);

        // 3. Obter os caminhos de instalação.
        const auto& paths =
            Blatman::Install::getInstallPaths();

        const auto& prefix = paths.PREFIX;

        if (!prefix.is_absolute())
            return fail(
                "o prefixo de instalação Meson precisa ser absoluto."
            );

        // 4. Configurar e compilar.
        if (!meson_detail::configureAndCompile(
                build,
                prefix,
                paths.BIN,
                paths.LIB,
                paths.INCLUDE,
                paths.SHARE
            ))
        {
            return fail(
                "falha ao configurar ou compilar o projeto Meson."
            );
        }

        // 5. Inspecionar o plano de instalação.
        const auto destinations =
            meson_detail::inspectMeson(build);

        // 6. Validar os destinos.
        meson_detail::validateDestinations(
            destinations,
            prefix
        );

        // 7. Instalar dentro do staging.
        if (!meson_detail::installMeson(build, stage))
            return fail(
                "falha durante a instalação Meson no staging."
            );

        // 8. Validar o resultado.
        if (!meson_detail::validateStage(stage, prefix))
            return fail(
                "o staging Meson não passou na validação final."
            );

        return true;
    }
    catch (const std::exception& error)
    {
        return installation_detail::fail(error.what());
    }
}