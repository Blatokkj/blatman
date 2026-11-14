#include "operations/installation/details/mozilla.hpp"
#include "shell.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace mozilla_detail
{

    fs::path packageMozilla(
        const fs::path& source,
        const BuildLayout& layout
    )
    {
        std::cout << "Mozilla Mach: gerando pacote...\n";

        const auto packaged = runShell(
            "cd " + quoteShell(source.string()) +
            " && ./mach package",
            true
        );

        if (!packaged.success)
            throw std::runtime_error(
                "falha ao gerar o pacote Mozilla.\n" +
                packaged.output
            );

        if (!fs::is_directory(layout.distributionDirectory))
            throw std::runtime_error(
                "diretório dist não foi criado pelo Mach."
            );

        std::vector<fs::path> archives;

        for (const auto& entry :
             fs::directory_iterator(layout.distributionDirectory))
        {
            if (!entry.is_regular_file())
                continue;

            const auto name = entry.path().filename().string();

            const bool firefoxPackage =
                name.starts_with("firefox") &&
                (name.ends_with(".tar.bz2") ||
                 name.ends_with(".tar.xz"));

            if (firefoxPackage)
                archives.push_back(entry.path());
        }

        if (archives.empty())
            throw std::runtime_error(
                "nenhum pacote tarball do Firefox foi encontrado em dist."
            );

        std::sort(
            archives.begin(),
            archives.end(),
            [](const fs::path& a, const fs::path& b)
            {
                return fs::last_write_time(a) >
                       fs::last_write_time(b);
            }
        );

        if (archives.size() > 1 &&
            fs::last_write_time(archives[0]) ==
            fs::last_write_time(archives[1]))
        {
            throw std::runtime_error(
                "há mais de um pacote Firefox com data de modificação indistinguível."
            );
        }

        return fs::canonical(archives.front());
    }

}