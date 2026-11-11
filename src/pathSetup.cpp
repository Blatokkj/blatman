#include "pathSetup.hpp"
#include "operations/installation/base/baseInstall.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

namespace Blatman::Install {

namespace fs = std::filesystem;

bool setupPath()
{
    const char *home = std::getenv("HOME");
    const char *shell = std::getenv("SHELL");
    const auto binPath = Blatman::Install::getInstallPaths().BIN;

    if (!home || !shell)
        return false;

    const std::string shellName = fs::path(shell).filename().string();

    fs::path rc;

    if (shellName == "bash")
        rc = fs::path(home) / ".bashrc";
    else if (shellName == "zsh")
        rc = fs::path(home) / ".zshrc";
    else
        return false;

    const std::string pathLine =
        "export PATH=\"" + binPath.string() + ":$PATH\"";

    // Verifica se ja esta configurado.
    if (fs::exists(rc)) {
        std::ifstream input(rc);
        std::string line;

        while (std::getline(input, line)) {
            if (line.find(binPath.string()) != std::string::npos)
                return true;
        }
    }

    // Adiciona a configuração.
    std::ofstream output(rc, std::ios::app);

    if (!output)
        return false;

    output << "\n# Blatman PATH\n"
           << pathLine << '\n';

    return output.good();
}

fs::path getHomePath()
{
    const char *home = std::getenv("HOME");

    if (!home || *home == '\0')
        throw std::runtime_error("Variável HOME não encontrada.");

    fs::path homePath(home);

    if (!homePath.is_absolute())
        throw std::runtime_error("O caminho HOME precisa ser absoluto.");

    return homePath;
}

}