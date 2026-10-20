#include "operations/install.hpp"
#include "shell.hpp"
#include "provider.hpp"
#include "temporaryDirectory.hpp"
#include "projectWorkflow.hpp"
#include "installTransaction.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <fstream>

std::optional<fs::path> Install::clone(const std::string &url, const fs::path &cacheRoot) const
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

    const std::string allowed = "abcdefghijklmnopqrstuvwxyz"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789._-";

    for (const auto &component : parts)
    {
        if (component.empty() || component == "." || component == ".." ||
            component.find_first_not_of(allowed) != std::string::npos)
        {
            std::cerr << "URL contém um componente inválido.\n";
            return std::nullopt;
        }
    }

    fs::create_directories(cacheRoot);
    fs::path parent = fs::canonical(cacheRoot);
    for (const auto &component : {parts[0], parts[1]})
    {
        parent /= component;
        if (fs::is_symlink(parent) || (fs::exists(parent) && !fs::is_directory(parent)))
        {
            std::cerr << "Diretorio de cache invalido: " << parent << '\n';
            return std::nullopt;
        }
        fs::create_directory(parent);
    }
    fs::path destination = parent / parts[2];

    if (fs::exists(destination) || fs::is_symlink(destination))
    {
        if (fs::is_symlink(destination) || !fs::is_directory(destination / ".git") ||
            fs::is_symlink(destination / ".git"))
        {
            std::cerr << "O destino existente não é um clone aceito.\n";
            return std::nullopt;
        }

        const std::string gitCommand = "git -C " + quoteShell(destination.string());

        const CommandResult head = runShell(gitCommand + " rev-parse --verify HEAD");

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
                   (value.back() == '\n' || value.back() == '\r' || value.back() == '/'))
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

        if (fs::is_regular_file(destination / ".gitmodules") &&
            !runShell(gitCommand + " submodule update --init --recursive").success)
            return std::nullopt;
        std::cout << "Reutilizando repositório: " << destination << '\n';
        return destination;
    }

    TemporaryDirectory work(destination.parent_path());
    const auto partial = work.path() / "repository";
    if (!runShell("git clone --recurse-submodules " + quoteShell(url) + " " +
                  quoteShell(partial.string()))
             .success)
        return std::nullopt;
    if (fs::exists(destination))
    {
        std::cerr << "O destino apareceu durante o clone. Tente novamente.\n";
        return std::nullopt;
    }
    fs::rename(partial, destination);

    return destination;
}

bool Install::installPackages(const std::vector<std::string> &packages) const
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
    const std::string allowed = "abcdefghijklmnopqrstuvwxyz"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789.+_-:@";

    for (const auto &package : packages)
    {
        if (package.empty() || package.front() == '-' || package.front() == '.' ||
            package.find_first_not_of(allowed) != std::string::npos || package.ends_with(".rpm") ||
            package.ends_with(".deb"))
        {
            std::cerr << "Nome de pacote invalido: " << package << '\n';
            return false;
        }

        if (manager == packageManager::Apt && (package.ends_with("-") || package.ends_with("+")))
        {
            std::cerr << "Informe apenas o nome do pacote, sem modificadores: " << package << '\n';
            return false;
        }
    }

    std::string command;

    if (manager != packageManager::Brew)
        command = "sudo ";

    command += info.command + " " + info.install;

    for (const auto &package : packages)
        command += " " + quoteShell(package);

    std::cout << "Executando: " << command << '\n';

    if (std::system(command.c_str()) != 0)
    {
        std::cerr << "Falha na instalação dos pacotes.\n";
        return false;
    }

    return true;
}

bool Install::execute(const std::string &argument) const
{
    if (const char *destination = std::getenv("BLATMAN_DESTDIR"); destination && *destination)
    {
        fs::path root(destination);
        if (!root.is_absolute())
        {
            std::cerr << "BLATMAN_DESTDIR precisa ser absoluto.\n";
            return false;
        }
        fs::create_directories(root);
        return installProject(argument, Install(fs::canonical(root)));
    }
    return installProject(argument, *this);
}
bool Install::publish(const fs::path &stage) const
{
    if (destinationRoot_ != "/")
        return publishTree(stage, destinationRoot_);
    const auto executable = fs::read_symlink("/proc/self/exe");
    return runShell("sudo -- " + quoteShell(executable.string()) + " --publish-stage " +
                    quoteShell(fs::absolute(stage).string()))
        .success;
}
