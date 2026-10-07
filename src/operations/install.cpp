#include "operations/install.hpp"
#include "shell.hpp"
#include "provider.hpp"
#include "temporaryDirectory.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <fstream>

namespace
{
struct InstallFile
{
    fs::path source;
    fs::path destination;
};

bool isWithin(const fs::path &path, const fs::path &root)
{
    const auto match = std::mismatch(root.begin(), root.end(), path.begin(), path.end());
    return match.first == root.end();
}

bool availableDestination(const fs::path &destination, bool directory)
{
    // Nao atravessa links em diretorios de destino nem substitui arquivos.
    fs::path current;
    for (const auto &component : destination)
    {
        current /= component;
        const auto status = fs::symlink_status(current);
        if (!fs::exists(status))
            continue;
        const bool needsDirectory = current != destination || directory;
        if (!needsDirectory || !fs::is_directory(status))
        {
            std::cerr << "Conflito no destino: " << current << '\n';
            return false;
        }
    }
    return true;
}

bool executeInstallCommand(const std::string &command)
{
    std::cout << "Executando: " << command << std::endl;
    return std::system(command.c_str()) == 0;
}

bool publishStage(const fs::path &stagePath)
{
    const fs::path stage = fs::canonical(stagePath);
    const fs::path stagedPrefix = stage / "usr";
    std::vector<InstallFile> files;
    std::vector<fs::path> directories;

    for (const auto &entry : fs::recursive_directory_iterator(stage))
    {
        // lexically_relative preserva o nome de um link, sem resolve-lo.
        const fs::path relative = entry.path().lexically_relative(stage);
        const fs::path destination = fs::path("/") / relative;
        if (!isWithin(destination, fs::path("/usr")))
        {
            std::cerr << "Receita tentou instalar fora de /usr: " << destination << '\n';
            return false;
        }

        const auto status = entry.symlink_status();
        if (fs::is_directory(status))
        {
            directories.push_back(destination);
            continue;
        }
        if (fs::is_symlink(status))
        {
            const fs::path link = fs::read_symlink(entry.path());
            fs::path target;
            if (link.is_absolute())
            {
                if (!isWithin(link.lexically_normal(), fs::path("/usr")))
                {
                    std::cerr << "Link fora de /usr: " << entry.path() << '\n';
                    return false;
                }
                target = stage / link.relative_path();
            }
            else
                target = entry.path().parent_path() / link;

            const auto resolved = fs::weakly_canonical(target);
            if (!isWithin(resolved, stagedPrefix) || !fs::is_regular_file(resolved))
            {
                std::cerr << "Link nao aponta para um arquivo desta instalacao: " << entry.path()
                          << '\n';
                return false;
            }
        }
        else if (!fs::is_regular_file(status))
        {
            std::cerr << "Tipo de arquivo nao suportado: " << entry.path() << '\n';
            return false;
        }
        else if ((status.permissions() & (fs::perms::set_uid | fs::perms::set_gid)) !=
                 fs::perms::none)
        {
            std::cerr << "Arquivo com permissoes especiais nao suportadas: " << entry.path()
                      << '\n';
            return false;
        }
        files.push_back({entry.path(), destination});
    }

    if (files.empty())
    {
        std::cerr << "A receita nao preparou arquivos para instalar.\n";
        return false;
    }

    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end(),
              [](const auto &a, const auto &b) { return a.destination < b.destination; });

    // Esta fase inteira termina antes do primeiro comando privilegiado.
    for (const auto &directory : directories)
        if (!availableDestination(directory, true))
            return false;
    for (const auto &file : files)
        if (!availableDestination(file.destination, false))
            return false;

    for (const auto &directory : directories)
    {
        if (!availableDestination(directory, true))
            return false;
        if (!fs::exists(directory) &&
            !executeInstallCommand("sudo mkdir -p -- " + quoteShell(directory.string())))
        {
            std::cerr << "Falha ao criar diretorios de instalacao.\n";
            return false;
        }
    }

    std::vector<fs::path> installed;
    for (const auto &file : files)
    {
        // GNU cp none-fail recusa sobrescrita, inclusive se surgir um novo
        // arquivo depois da verificacao inicial. Nao preserva o dono do stage.
        const std::string command =
            "sudo cp --no-dereference --preserve=mode --update=none-fail -T -- " +
            quoteShell(file.source.string()) + " " + quoteShell(file.destination.string());
        if (!availableDestination(file.destination, false) || !executeInstallCommand(command))
        {
            std::cerr << "Instalacao incompleta; verifique o destino: " << file.destination << '\n';
            for (const auto &previous : installed)
                std::cerr << "Ja instalado: " << previous << '\n';
            return false;
        }
        installed.push_back(file.destination);
    }
    return true;
}

bool executableSource(const fs::path &root, const fs::path &relative)
{
    if (relative.empty() || relative.is_absolute())
    {
        std::cerr << "Informe um executavel relativo ao projeto.\n";
        return false;
    }
    const fs::path candidate = root / relative;
    if (fs::is_symlink(candidate))
    {
        std::cerr << "O executavel deve ser um arquivo real.\n";
        return false;
    }
    const fs::path source = fs::canonical(candidate);
    if (!isWithin(source, root) || !fs::is_regular_file(source))
    {
        std::cerr << "Executavel invalido ou fora do projeto: " << relative << '\n';
        return false;
    }
    std::ifstream file(source, std::ios::binary);
    char header[4]{};
    file.read(header, sizeof(header));
    const auto bytes = file.gcount();
    const bool elf = bytes == 4 && header[0] == '\x7f' && header[1] == 'E' && header[2] == 'L' &&
                     header[3] == 'F';
    const bool script = bytes >= 2 && header[0] == '#' && header[1] == '!';
    if (!elf && !script)
    {
        std::cerr << "Formato de executavel nao suportado: " << relative << '\n';
        return false;
    }
    return true;
}
} // namespace

std::optional<fs::path> Install::clone(
    const std::string& url,
    const fs::path& cacheRoot) const
{
    if (!url.starts_with("https://"))
    {
        std::cerr << "Informe uma URL HTTPS.\n";
        return std::nullopt;
    }
    std::istringstream stream(url.substr(8));
    std::vector<std::string> parts;
    std::string part;

    while (std::getline(stream, part, '/'))
        parts.push_back(part);

    if (parts.size() != 3)
    {
        std::cerr << "Use https://servidor/autor/repositorio.git\n";
        return std::nullopt;
    }

    // remove o sufixo ".git"
    if (parts[2].ends_with(".git"))
        parts[2].resize(parts[2].size() - 4);

    const std::string allowed =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789._-";

    for (const auto& component : parts)
    {
        if (component.empty() ||
            component == "." ||
            component == ".." ||
            component.find_first_not_of(allowed) != std::string::npos)
        {
            std::cerr << "URL contém um componente inválido.\n";
            return std::nullopt;
        }
    }

    fs::path destination = 
        cacheRoot / parts[0] / parts[1] / parts[2];

    if (fs::exists(destination) || fs::is_symlink(destination))
    {
        if (fs::is_symlink(destination) ||
            !fs::is_directory(destination / ".git") ||
            fs::is_symlink(destination / ".git"))
        {
            std::cerr << "O destino existente não é um clone aceito.\n";
            return std::nullopt;
        }

        const std::string gitCommand =
            "git -C " + quoteShell(destination.string());

        const CommandResult head =
            runShell(gitCommand + " rev-parse --verify HEAD");

        const CommandResult origin =
            runShell(gitCommand + " config --local --get remote.origin.url");

        if (!head.success || !origin.success)
        {
            std::cerr << "Não foi possivel validar o clone existente.\n";
            return std::nullopt;
        }

        const auto normalizeUrl = [](std::string value)
        {
            while (!value.empty() &&
                   (value.back() == '\n' ||
                    value.back() == '\r' ||
                    value.back() == '/'))
            {
                value.pop_back();
            }

            if (value.ends_with(".git"))
                value.resize(value.size() - 4);

            return value;
        };

        if (normalizeUrl(origin.output) != normalizeUrl(url))
        {
            std::cerr << "O cache pertence a outro repositorio.\n";
            return std::nullopt;
        }

        std::cout << "Reutilizando repositório: " << destination << '\n';
        return destination;
    }

    fs::create_directories(destination.parent_path());

    std::cout << "Destino: " << destination << '\n';

    std::string command =
        "git clone " + quoteShell(url) + " " + quoteShell(destination.string());
        
    if (std::system(command.c_str()) != 0)
    {
        std::cerr << "Falha ao clonar o repositorio.\n";
        return std::nullopt;
    }

    return destination;
}

bool Install::installExecutable(const fs::path &repositoryPath,
                                const fs::path &relativeExecutable) const
{
    return installExecutables(repositoryPath, {relativeExecutable});
}

bool Install::installExecutables(const fs::path &repositoryPath,
                                 const std::vector<fs::path> &relativeExecutables) const
{
    if (relativeExecutables.empty())
    {
        std::cerr << "Nenhum executavel informado.\n";
        return false;
    }
    const fs::path root = fs::canonical(repositoryPath);
    TemporaryDirectory workspace;
    const fs::path bin = workspace.path() / "usr" / "bin";
    fs::create_directories(bin);

    for (const auto &relative : relativeExecutables)
    {
        if (!executableSource(root, relative))
            return false;
        const fs::path destination = bin / relative.filename();
        if (fs::exists(destination))
        {
            std::cerr << "Executaveis com nomes repetidos: " << relative.filename() << '\n';
            return false;
        }
        fs::copy_file(root / relative, destination);
        fs::permissions(destination, fs::perms::owner_all | fs::perms::group_read |
                                         fs::perms::group_exec | fs::perms::others_read |
                                         fs::perms::others_exec);
    }
    return publishStage(workspace.path());
}

bool Install::installPackages(
    const std::vector<std::string>& packages) const
{
    if (packages.empty())
    {
        std::cerr << "Nenhum pacote informado.\n";
        return false;
    }

    const packageManager manager = detectPackageManager();

    if (manager == packageManager::Unknown)
    {
        std::cerr << "Nenhum gerenciador de pacotes suportado encontrado.\n";
        return false;
    }

    const packageManagerInfo info = getPackageManagerInfo(manager);

    // Aceitamos nomes simples de pacotes, sem caminhos ou opcoes.
    const std::string allowed =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789.+_-:@";

    for (const auto& package : packages)
    {
        if (package.empty() ||
            package.front() == '-' ||
        package.front() == '.' ||
            package.find_first_not_of(allowed) != std::string::npos ||
            package.ends_with(".rpm") ||
            package.ends_with(".deb"))
        {
            std::cerr << "Nome de pacote invalido: " << package << '\n';
            return false;
        }

        if (manager == packageManager::Apt &&
            (package.ends_with("-") || package.ends_with("+")))
        {
            std::cerr
                << "Informe apenas o nome do pacote, sem modificadores: "
                << package << '\n';
            return false;
        }
    }

    std::string command;

    if (manager != packageManager::Brew)
        command = "sudo ";

    command += info.command + " " + info.install;

    for (const auto& package : packages)
        command += " " + quoteShell(package);

    std::cout << "Executando: " << command << '\n';

    if (std::system(command.c_str()) != 0)
    {
        std::cerr << "Falha na instalação dos pacotes.\n";
        return false;
    }

    return true;
}

bool Install::installCMake(const fs::path &buildDirectory) const
{
    const fs::path directory = fs::canonical(buildDirectory);
    if (!fs::is_regular_file(directory / "cmake_install.cmake"))
    {
        std::cerr << "Receita de instalacao CMake nao encontrada.\n";
        return false;
    }

    TemporaryDirectory workspace;
    const std::string command = "DESTDIR=" + quoteShell(workspace.path().string()) +
                                " cmake --install " + quoteShell(directory.string()) +
                                " --prefix /usr";

    std::cout << "Preparando instalacao: " << command << '\n';
    if (!runShell(command).success)
        return false;

    return publishStage(workspace.path());
}
