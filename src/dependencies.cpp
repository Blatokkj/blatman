#include "dependencies.hpp"
#include "operations/install.hpp"

#include <iostream>
#include <regex>
#include <set>
#include <sstream>

std::vector<std::string> findMissingDependencies(
    const std::string& output)
{
    const std::vector<std::regex> patterns = {
        // pkg-config/pkgconf.
        std::regex(
            R"rx(Package '([^']+)'(?:, required by '[^']*',)? not found)rx"
        ),

        std::regex(
            R"rx(Package ([^ \r\n]+) was not found in the pkg-config search path\.)rx"
        ),

        // CMake: arquivo de configuracao de um pacote nao encontrado.
        std::regex(
            R"rx(Could not find a package configuration file provided by[ \t\r\n]+"([^"]+)")rx"
        )
    };

    std::set<std::string> dependencies;

    for (const auto& pattern : patterns)
    {
        const std::sregex_iterator end;

        for (std::sregex_iterator match(
                 output.begin(), output.end(), pattern);
             match != end;
             ++match)
        {
            dependencies.insert((*match)[1].str());
        }
    }

    return std::vector<std::string>(
        dependencies.begin(), dependencies.end());
}

bool resolveDependency(const std::string& dependency)
{
    std::cout
        << "O diagnostico menciona um requisito nao localizado: "
        << dependency << '\n'
        << "Isso pode ser opcional ou um problema no caminho de busca.\n";

    std::string answer;

    // Primeira pergunta: deseja tentar resolver esse requisito?
    while (true)
    {
        std::cout
            << "Deseja tentar resolve-lo instalando uma dependencia? "
            << "[Y/N]: ";

        if (!std::getline(std::cin, answer))
            return false;

        if (answer == "Y" || answer == "y" ||
            answer == "S" || answer == "s")
        {
            break;
        }

        if (answer == "N" || answer == "n")
            return false;

        std::cout << "Responda Y ou N.\n";
    }

    // Segunda pergunta: existe um pacote nos repositorios?
    while (true)
    {
        std::cout
            << "Existe um pacote nos repositorios oficiais "
            << "que forneca esse requisito? [Y/N]: ";

        if (!std::getline(std::cin, answer))
            return false;

        if (answer == "Y" || answer == "y" ||
            answer == "S" || answer == "s")
        {
            break;
        }

        if (answer == "N" || answer == "n")
        {
            std::cerr
                << "A instalacao completa de dependencias por URL "
                << "ainda nao esta implementada.\n";
            return false;
        }

        std::cout << "Responda Y ou N.\n";
    }

    std::cout << "Informe os nomes dos pacotes separados por espacos: ";

    std::string line;

    if (!std::getline(std::cin, line))
        return false;

    std::istringstream input(line);
    std::vector<std::string> packages;
    std::string package;

    while (input >> package)
        packages.push_back(package);

    Install installer;
    return installer.installPackages(packages);
}