#include "operations/installation/details/mozilla.hpp"

#include "operations/installation/details/installationHelpers.hpp"

#include <stdexcept>
#include <string>

namespace mozilla_detail
{

bool validateMozilla(
    const fs::path& stage,
    const fs::path& prefix
)
{
    using installation_detail::fail;
    using installation_detail::within;

    if (!installation_detail::populated(stage))
        return false;

    if (!prefix.is_absolute() ||
        prefix == prefix.root_path() ||
        prefix != prefix.lexically_normal())
    {
        return fail("Prefixo inválido durante a validação.");
    }

    const auto root = fs::canonical(stage);

    const auto stagedPrefix = (
        root / prefix.relative_path()
    ).lexically_normal();

    if (!within(stagedPrefix, root))
        return fail("O prefixo escapa do diretório de staging.");

    std::error_code error;

    const auto prefixStatus =
        fs::symlink_status(stagedPrefix, error);

    if (error ||
        prefixStatus.type() != fs::file_type::directory ||
        fs::canonical(stagedPrefix) != stagedPrefix)
    {
        return fail(
            "A instalação não produziu o diretório de prefixo esperado."
        );
    }

    std::size_t files = 0;

    for (fs::recursive_directory_iterator it(root), end;
         it != end;
         ++it)
    {
        const auto entry = it->path().lexically_normal();

        const bool inside =
            within(entry, stagedPrefix);

        const bool ancestor =
            within(stagedPrefix, entry);

        if (!inside && !ancestor)
        {
            return fail(
                "Arquivo encontrado fora do prefixo: " +
                entry.string()
            );
        }

        const auto status = fs::symlink_status(entry);
        const auto type = status.type();

        // Os diretórios ancestrais do prefixo precisam ser reais.
        if (ancestor && !inside &&
            type != fs::file_type::directory)
        {
            return fail(
                "O caminho até o prefixo contém um elemento inválido: " +
                entry.string()
            );
        }

        if (type != fs::file_type::directory &&
            type != fs::file_type::regular &&
            type != fs::file_type::symlink)
        {
            return fail(
                "Tipo de arquivo não suportado: " +
                entry.string()
            );
        }

        if (type != fs::file_type::symlink &&
            (status.permissions() &
             (fs::perms::set_uid | fs::perms::set_gid)) !=
                fs::perms::none)
        {
            return fail(
                "Permissões especiais não suportadas: " +
                entry.string()
            );
        }

        if (type == fs::file_type::symlink)
        {
            const auto target = fs::read_symlink(entry);

            const auto resolved = (
                target.is_absolute()
                    ? target
                    : entry.parent_path() / target
            ).lexically_normal();

            const bool validTarget = target.is_absolute()
                ? within(resolved, prefix.lexically_normal())
                : within(resolved, stagedPrefix);

            if (!validTarget)
            {
                return fail(
                    "Link simbólico escapa do prefixo: " +
                    entry.string()
                );
            }
        }

        if (type != fs::file_type::directory)
            ++files;
    }

    if (files == 0)
        return fail("Nenhum arquivo instalável foi produzido.");

    return true;
}

}