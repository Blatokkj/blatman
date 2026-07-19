#include <iostream>
#include <filesystem>
#include <regex>
#include "ArgumentParser.hpp"
#include "GitRepository.hpp"
#include "BuildDetector.hpp"
#include "Builder.hpp"
#include "Logger.hpp"
#include "Installer.hpp"

namespace fs = std::filesystem;

std::string sanitizeRepoName(const std::string& url) {
    std::regex githubPattern(R"(^https://github\.com/[A-Za-z0-9_.-]+/([A-Za-z0-9_.-]+)(\.git)?/?$)");
    std::smatch matches;
    if (std::regex_match(url, matches, githubPattern) && matches.size() > 1) {
        std::string name = matches[1].str();
        name = std::regex_replace(name, std::regex(R"([^\w.-])"), "_");
        return name;
    }
    return "unknown_repo";
}

void printHelp() {
    std::cout << "Blatman - GitHub Project Builder\n";
    std::cout << "\nUso:\n";
    std::cout << "  blatman install <URL>    Clona, compila e instala um projeto do GitHub\n";
    std::cout << "  blatman list             Lista todos os pacotes instalados\n";
    std::cout << "  blatman upgrade [nome]   Atualiza um pacote específico ou todos\n";
    std::cout << "  blatman packs            Mostra a quantidade de pacotes instalados\n";
    std::cout << "  blatman help             Mostra esta ajuda\n";
    std::cout << "\nExemplos:\n";
    std::cout << "  blatman install https://github.com/usuario/repo\n";
    std::cout << "  blatman list\n";
    std::cout << "  blatman upgrade\n";
    std::cout << "  blatman upgrade lazygit\n";
    std::cout << "  blatman packs\n";
}

int main(int argc, char* argv[]) {
    const fs::path homeDir = getenv("HOME") ? getenv("HOME") : "/tmp";
    const fs::path blatmanDir = homeDir / ".blatman";
    const fs::path logDir = blatmanDir / "logs";
    const fs::path cacheDir = blatmanDir / "cache";
    const fs::path binDir = blatmanDir / "bin";
    const fs::path manifestDir = blatmanDir / "manifests";

    Logger logger(logDir);
    logger.logInfo("Iniciando Blatman v0.1");

    try {
        ArgumentParser parser(argc, argv);
        Command cmd = parser.getCommand();

        if (cmd == Command::Help || cmd == Command::Unknown) {
            printHelp();
            return cmd == Command::Help ? 0 : 1;
        }

        if (cmd == Command::List) {
            Installer installer(cacheDir, binDir, manifestDir);
            auto packages = installer.listInstalledPackages();
            if (packages.empty()) {
                std::cout << "Nenhum pacote instalado." << std::endl;
            } else {
                std::cout << "Pacotes instalados (" << packages.size() << "):\n";
                for (const auto& pkg : packages) {
                    std::cout << "  - " << pkg.name << " [" << pkg.buildSystem << "]\n";
                }
            }
            return 0;
        }

        if (cmd == Command::Packs) {
            Installer installer(cacheDir, binDir, manifestDir);
            size_t count = installer.getPackageCount();
            std::cout << count << " pacote(s) instalado(s)." << std::endl;
            return 0;
        }

        if (cmd == Command::Upgrade) {
            Installer installer(cacheDir, binDir, manifestDir);
            std::string packageName = parser.getPackageName();
            if (!packageName.empty()) {
                return installer.upgradePackage(packageName) ? 0 : 1;
            } else {
                return installer.upgradeAllPackages() ? 0 : 1;
            }
        }

        if (cmd == Command::Install) {
            std::string url = parser.getUrl();
            logger.logInfo("URL do repositório: " + url);

            std::string repoName = sanitizeRepoName(url);
            fs::path repoPath = cacheDir / repoName;

            GitRepository git;
            if (!git.clone(url, repoPath)) {
                logger.logError("Falha ao clonar repositório: " + url);
                std::cerr << "Erro: Falha ao clonar repositório." << std::endl;
                return 1;
            }
            logger.logInfo("Repositório clonado em: " + repoPath.string());

            BuildDetector detector;
            BuildSystem buildSystem = detector.detect(repoPath);
            if (buildSystem == BuildSystem::Unknown) {
                logger.logError("Sistema de build desconhecido em: " + repoPath.string());
                std::cerr << "Erro: Sistema de build desconhecido." << std::endl;
                return 1;
            }
            logger.logInfo("Sistema de build detectado: " + std::to_string(static_cast<int>(buildSystem)));

            Builder builder;
            logger.logInfo("Iniciando compilação com sistema de build: " + std::to_string(static_cast<int>(buildSystem)));
            if (!builder.build(buildSystem, repoPath)) {
                logger.logError("Falha na compilação do projeto em: " + repoPath.string());
                std::cerr << "Erro: Falha na compilação." << std::endl;
                return 1;
            }
            logger.logInfo("Compilação e instalação concluídas com sucesso.");
            std::cout << "Sucesso: Compilação e instalação concluídas." << std::endl;
            return 0;
        }

    } catch (const std::exception& e) {
        logger.logError("Exceção: " + std::string(e.what()));
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}