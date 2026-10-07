#include "operations/install.hpp"
#include "shell.hpp"
#include "provider.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <fstream>

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

bool Install::installExecutable(
    const fs::path& repositoryPath,
    const fs::path& relativeExecutable) const
{
    if (relativeExecutable.empty() ||
        relativeExecutable.is_absolute())
    {
        std::cerr << "Informe um caminho relativo ao projeto.\n";
        return false;
    }

    const fs::path root = fs::canonical(repositoryPath);
    const fs::path candidate = root / relativeExecutable;

    if (fs::is_symlink(candidate))
    {
        std::cerr << "Selecione o arquivo real, nao um link simbolico.\n";
        return false;
    }

    const fs::path source = fs::canonical(candidate);

    // Compara componentes de caminho, nao prefixos de texto.
    const auto comparison = std::mismatch(
        root.begin(), root.end(),
        source.begin(), source.end());

    if (comparison.first != root.end())
    {
        std::cerr << "O arquivo esta fora do diretorio do projeto.\n";
        return false;
    }

    if (!fs::is_regular_file(source))
    {
        std::cerr << "O caminho nao corresponde a um arquivo regular.\n";
        return false;
    }

    // Triagem inicial para binarios ELF e scripts com shebang.
    std::ifstream file(source, std::ios::binary);

    if (!file)
    {
        std::cerr << "Nao foi possivel ler o arquivo selecionado.\n";
        return false;
    }

    char header[4]{};
    file.read(header, sizeof(header));
    const std::streamsize bytesRead = file.gcount();

    const bool isElf =
        bytesRead == 4 &&
        header[0] == '\x7f' &&
        header[1] == 'E' &&
        header[2] == 'L' &&
        header[3] == 'F';

    const bool isScript =
        bytesRead >= 2 &&
        header[0] == '#' &&
        header[1] == '!';

    if (!isElf && !isScript)
    {
        std::cerr << "Formato nao aceito: esperado ELF ou script com #!.\n";
        return false;
    }

    const fs::path destination =
        fs::path("/usr/bin") / source.filename();

    if (fs::exists(destination) || fs::is_symlink(destination))
    {
        std::cerr << "O destino ja existe: " << destination << '\n';
        return false;
    }

    std::cout << "Instalando em: " << destination << '\n';

    const std::string command =
        "sudo install -m 755 -T -- " +
        quoteShell(source.string()) + " " +
        quoteShell(destination.string());

    if (std::system(command.c_str()) != 0)
    {
        std::cerr << "Falha ao instalar o executavel.\n";
        return false;
    }

    return true;
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
        std::cerr << "Falha na instalacao dos pacotes.\n";
        return false;
    }

    return true;
}

bool Install::installCMake(const fs::path& buildDirectory) const
{
    const fs::path directory = fs::absolute(buildDirectory);

    if (!fs::is_regular_file(directory / "cmake_install.cmake"))
    {
        std::cerr << "Receita de instalação CMake não encontrada.\n";
        return false;
    }

    const std::string command =
        "sudo cmake --install " +
        quoteShell(directory.string()) +
        " --prefix /usr";

    std::cout << "Executando: " << command << '\n';

    return runShell(command).success;
}