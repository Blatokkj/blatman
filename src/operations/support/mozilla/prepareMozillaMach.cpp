#include "operations/installation/details/mozilla.hpp"
#include "operations/installation/nativeInstall.hpp"
#include "operations/installation/base/baseInstall.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

bool prepareMozillaMach(
    const fs::path& sourceDir,
    const fs::path& stage
)
{
    using namespace installation_detail;

    try
    {
        if (!validStage(stage))
            return false;

        const auto source = directory(sourceDir);

        const auto& prefix =
            Blatman::Install::getInstallPaths().PREFIX;

        if (!prefix.is_absolute())
        {
            return fail(
                "O prefixo de instalação precisa ser absoluto."
            );
        }

        // 1. Descobrir o ambiente real do Mach.
        const auto environment =
            mozilla_detail::inspectMozilla(source);

        // 2. Verificar se há uma interface de instalação.
        if (!mozilla_detail::inspectInstallMozilla(environment))
            return false;

        // 3. Executar a instalação sem publicar no sistema.
        if (!mozilla_detail::stageMozilla(
                environment,
                stage,
                prefix
            ))
        {
            return false;
        }

        // 4. Validar o resultado antes da publicação.
        if (!mozilla_detail::validateMozilla(stage, prefix))
            return false;

        return true;
    }
    catch (const std::exception& error)
    {
        return fail(error.what());
    }
}