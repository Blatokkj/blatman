#include "provider.hpp"

packageManagerInfo getPackageManagerInfo(packageManager manager) 
{ // se detectar um gerenciador de pacotes, usa ele pra instalar, remover ou atualizar algo do repositório.   
    switch (manager)
    {
        case packageManager::Pacman:
            return {
                "pacman",
                "-S",
                "-R",
                "-Syu"
            };

        case packageManager::Apt:
            return {
                "apt",
                "install",
                "remove",
                "update"
            };

        case packageManager::Dnf:
            return {
                "dnf",
                "install",
                "remove",
                "upgrade"
            };

        case packageManager::Zypper:
            return {
                "zypper",
                "install",
                "remove",
                "update"
            };

        case packageManager::Brew:
            return {
                "brew",
                "install",
                "uninstall",
                "update"
            };

        case packageManager::Unknown:
            return {};
    }

    return {};
}