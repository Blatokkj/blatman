#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "operations/installation/details/installationHelpers.hpp"

namespace fs = std::filesystem;

namespace meson_detail
{

std::size_t validateDestinations(
    const std::vector<std::string>& destinations,
    const fs::path& prefix
)
{
    if (destinations.empty())
    {
        throw std::runtime_error(
            "nenhum destino de instalação foi encontrado no projeto Meson."
        );
    }

    std::size_t validated = 0;

    for (const auto& raw : destinations)
    {
        if (raw.empty() ||
            raw.find('\0') != std::string::npos)
        {
            throw std::runtime_error(
                "destino vazio ou inválido no plano Meson."
            );
        }

        const fs::path destination(raw);

        if (!destination.is_absolute())
        {
            throw std::runtime_error(
                "destino Meson não absoluto: " + raw
            );
        }

        const auto normalized =
            destination.lexically_normal();

        const auto normalizedPrefix =
            prefix.lexically_normal(); 

        if (normalized == normalizedPrefix ||
            !installation_detail::within(normalized, normalizedPrefix))
        {
            throw std::runtime_error(
                "destino fora do prefixo do Blatman: " +
                destination.string()
            );
        }

        ++validated;
    }

    return validated;
}

bool validateStage(
    const fs::path& stage,
    const fs::path& prefix
)
{
    if (!installation_detail::populated(stage))
        return false;

    const auto stageRoot = fs::canonical(stage);

    const auto stagedPrefix = (
        stageRoot / prefix.relative_path()
    ).lexically_normal();

    if (!installation_detail::within(stagedPrefix, stageRoot) ||
        fs::symlink_status(stagedPrefix).type() !=
            fs::file_type::directory ||
        fs::canonical(stagedPrefix) != stagedPrefix)
    {
        throw std::runtime_error(
            "o staging não contém o prefixo esperado do Blatman."
        );
    }

    // Impede que o projeto publique arquivos fora do prefixo.
    // Os diretórios ancestrais do prefixo são permitidos.

    for (fs::recursive_directory_iterator it(stageRoot), end;
         it != end;
         ++it)
    {
        const auto entry = it->path().lexically_normal();

        const bool inside =
            installation_detail::within(entry, stagedPrefix);

        const bool ancestor =
            installation_detail::within(stagedPrefix, entry);

        if (!inside && !ancestor)
        {
            throw std::runtime_error(
                "arquivo encontrado fora do prefixo no staging: " +
                entry.string()
            );
        }

        const auto status = fs::symlink_status(entry);

        if (status.type() == fs::file_type::symlink)
        {
            const auto target = fs::read_symlink(entry);

            const auto resolved = (
                target.is_absolute()
                    ? target
                    : entry.parent_path() / target
            ).lexically_normal();

            if (!installation_detail::within(resolved, stagedPrefix) &&
                !installation_detail::within(resolved, prefix))
            {
                throw std::runtime_error(
                    "link simbólico escapa do prefixo: " +
                    entry.string()
                );
            }
        }
    }

    return true;
}

}
