#include "commands.hpp"
#include "operations/install.hpp"
#include "builderDetector.hpp"
#include "operations/build.hpp"
#include "dependencies.hpp"
#include "temporaryDirectory.hpp"

#include <iostream>
#include <filesystem>
#include <set>
#include <vector>
#include <optional>

namespace fs = std::filesystem;

void printHelp()
{
    std::cout << R"(
Blatman - Universal Package Manager

Uso:
  blatman <comando> [opções] [argumentos]

Comandos:
  install <URL>          Compila e instala um repositorio (CMake/Go)
  remove <pacote>        Ainda nao implementado
  update [pacote]        Ainda nao implementado
  build <projeto>        Detecta e executa o sistema de build
  help                   Mostra esta ajuda

Opções:
  -h, --help             Mostra esta ajuda

Exemplos:
  blatman install https://github.com/fmtlib/fmt.git
  blatman remove firefox
  blatman update
  blatman build .
)";
}

bool installCommand(const std::string &argument)
{
    if (argument.empty())
    {
        std::cerr << "Erro: informe a URL do repositório.\n";
        return false;
    }

    try
    {
        fs::path cacheRoot = fs::current_path() / "blatman-cache";

        Install installer;

        std::cout << "Clonando: " << argument << '\n';

        const auto repositoryPath = installer.clone(argument, cacheRoot);

        if (!repositoryPath)
        {
            std::cerr << "Operação de instalação interrompida.\n";
            return false;
        }

        std::cout << "Repositório pronto para compilação.\n";
        std::cout << "Procurando sistema de build...\n";

        BuildDetector detector;
        BuildSystem system = detector.detect(*repositoryPath);

        const char *name = "";

        switch (system)
        {
        case BuildSystem::CMake:
            name = "CMake";
            break;
        case BuildSystem::Make:
            name = "Make";
            break;
        case BuildSystem::Meson:
            name = "Meson";
            break;
        case BuildSystem::Cargo:
            name = "Cargo";
            break;
        case BuildSystem::Npm:
            name = "Npm";
            break;
        case BuildSystem::Maven:
            name = "Maven";
            break;
        case BuildSystem::Gradle:
            name = "Gradle";
            break;
        case BuildSystem::Autotools:
            name = "Autotools";
            break;
        case BuildSystem::Go:
            name = "Go";
            break;
        case BuildSystem::Unknown:
            std::cout << "Nenhum sistema de build reconhecido.\n";
            return false;
        }

        std::cout << "Sistema detectado: " << name << '\n';

        fs::path executableDirectory;
        std::optional<TemporaryDirectory> executableWorkspace;

        if (system == BuildSystem::Go)
        {
            executableWorkspace.emplace(fs::absolute(*repositoryPath));
            executableDirectory = executableWorkspace->path();
        }

        Build builder;

        // Guarda os conjuntos de requisitos que ja tentamos resolver.
        std::set<std::vector<std::string>> attemptedDependencies;

        while (true)
        {
            const CommandResult result = builder.run(*repositoryPath, system, executableDirectory);

            if (result.success)
                break;

            const std::vector<std::string> missing = findMissingDependencies(result.output);

            if (missing.empty())
            {
                std::cerr << "Build falhou, mas nenhum diagnostico de dependencia "
                          << "suportado foi reconhecido. Confira a saida acima.\n";
                return false;
            }

            if (attemptedDependencies.contains(missing))
            {
                std::cerr << "Os mesmos requisitos continuam nao localizados "
                          << "após uma tentativa de resolução.\n";
                return false;
            }

            attemptedDependencies.insert(missing);

            for (const auto &dependency : missing)
            {
                if (!resolveDependency(dependency))
                {
                    std::cerr << "Resolução interrompida.\n";
                    return false;
                }
            }

            std::cout << "Tentando compilar novamente...\n";
        }

        std::cout << "Compilação concluida!\n";
        if (system == BuildSystem::CMake)
        {
            if (!installer.installCMake(*repositoryPath / "build"))
            {
                std::cerr << "Falha na instalação do projeto.\n";
                return false;
            }

            std::cout << "Instalação concluida!\n";
            return true;
        }
        if (system != BuildSystem::Go)
        {
            std::cerr << "Compilação concluída, mas a instalação automática "
                      << "deste sistema ainda nao foi implementada.\n";
            return false;
        }

        std::vector<fs::path> executables;

        for (const auto &entry : fs::directory_iterator(executableDirectory))
        {
            if (entry.is_symlink() || !entry.is_regular_file())
                continue;

            executables.push_back(fs::relative(entry.path(), *repositoryPath));
        }

        if (executables.empty())
        {
            std::cerr << "Nenhum executavel foi gerado nos pacotes selecionados. "
                      << "Instalação interrompida.\n";
            return false;
        }

        if (!installer.installExecutables(*repositoryPath, executables))
        {
            std::cerr << "Instalação interrompida.\n";
            return false;
        }

        std::cout << "Instalação concluida!\n";
        return true;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Erro durante a instalação: " << error.what() << '\n';
        return false;
    }
}

bool removeCommand(const std::string &)
{
    std::cerr << "Remove ainda nao implementado.\n";
    return false;
}

bool updateCommand(const std::string &)
{
    std::cerr << "Update ainda nao implementado.\n";
    return false;
}

bool buildCommand(const std::string &argument)
{
    if (argument.empty())
    {
        std::cerr << "Erro: informe o diretorio do projeto.\n";
        return false;
    }

    const fs::path projectPath = argument;

    try
    {
        if (!fs::is_directory(projectPath))
        {
            std::cerr << "Erro: informe um diretorio existente.\n";
            return false;
        }

        BuildDetector detector;
        const BuildSystem system = detector.detect(projectPath);

        Build builder;

        const CommandResult result = builder.run(projectPath, system);
        if (!result.success)
            return false;

        std::cout << "Build concluido com sucesso!\n";
        return true;
    }
    catch (const fs::filesystem_error &error)
    {
        std::cerr << "Erro de filesystem: " << error.what() << '\n';
        return false;
    }
}

Command parseCommand(const std::string& command)
{
    if (command == "help" ||
        command == "--help" ||
        command == "-h")
        return Command::Help;

    if (command == "install" ||
        command == "-S" ||
        command == "-i")
        return Command::Install;

    if (command == "remove" ||
        command == "-R" ||
        command == "-r")
        return Command::Remove;

    if (command == "update" ||
        command == "-U" ||
        command == "-u")
        return Command::Update;

    if (command == "build" ||
        command == "-B" ||
        command == "-b")
        return Command::Build;

    return Command::Unknown;
}

bool executeCommand(Command command, const std::string &argument)
{
    switch (command)
    {
    case Command::Help:
        printHelp();
        return true;
    case Command::Install:
        return installCommand(argument);
    case Command::Remove:
        return removeCommand(argument);
    case Command::Update:
        return updateCommand(argument);
    case Command::Build:
        return buildCommand(argument);
    case Command::Unknown:
        std::cerr << "Comando desconhecido.\n";
        return false;
    }
    return false;
}
