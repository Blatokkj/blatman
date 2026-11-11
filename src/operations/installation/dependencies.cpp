
#include "operations/installation/dependencies.hpp"
#include "operations/installation/install.hpp"
#include "operations/installation/requirementsParser.hpp"

#include <iostream>
#include <sstream>
#include <vector>

bool resolveDependency(
    const std::string &dependency,
    const std::function<bool(const std::string &)> &installUrl)
{
    std::cout << "O diagnóstico menciona um requisito não localizado: "
              << dependency << '\n'
              << "Isso pode ser opcional ou um problema no caminho de busca.\n";

    std::string answer;

    while (true)
    {
        std::cout << "Deseja tentar resolvê-lo instalando uma dependência? "
                  << "[Y/N]: ";

        if (!std::getline(std::cin, answer))
            return false;

        if (answer == "Y" || answer == "y" ||
            answer == "S" || answer == "s")
            break;

        if (answer == "N" || answer == "n")
            return false;

        std::cout << "Responda Y ou N.\n";
    }

    while (true)
    {
        std::cout << "Existe um pacote nos repositórios oficiais "
                  << "que forneça esse requisito? [Y/N]: ";

        if (!std::getline(std::cin, answer))
            return false;

        if (answer == "Y" || answer == "y" ||
            answer == "S" || answer == "s")
            break;

        if (answer == "N" || answer == "n")
        {
            if (!installUrl)
                return false;

            std::cout << "Informe as URLs HTTPS separadas por espaços: ";

            std::string line, url;

            if (!std::getline(std::cin, line))
                return false;

            std::istringstream input(line);
            std::vector<std::string> urls;

            while (input >> url)
            {
                if (!url.starts_with("https://"))
                {
                    std::cerr << "URL inválida.\n";
                    return false;
                }

                urls.push_back(url);
            }

            if (urls.empty())
                return false;

            for (const auto &item : urls)
                if (!installUrl(item))
                    return false;

            return true;
        }

        std::cout << "Responda Y ou N.\n";
    }

    std::cout << "Informe os nomes dos pacotes separados por espaços: ";

    std::string line;

    if (!std::getline(std::cin, line))
        return false;

    std::istringstream input(line);
    std::vector<std::string> packages;
    std::string package;

    while (input >> package)
        packages.push_back(package);

    if (packages.empty())
        return false;

    Install installer;
    return installer.installPackages(packages);
}
