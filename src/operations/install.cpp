#include "operations/install.hpp"
#include "shell.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

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

    if (fs::exists(destination))
    {
        std::cerr << "O destino já existe: " << destination << '\n';
        return std::nullopt;
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