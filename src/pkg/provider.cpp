#include "pkg/provider.hpp"

packageManagerInfo getPackageManagerInfo(packageManager manager) 
{ // se detectar um gerenciador de pacotes, usa ele pra instalar, remover ou atualizar algo do repositório.   
    switch (manager)
    {
        case packageManager::Pacman:
            return {
                "pacman",
                "-S",
                "pacman",
                "-R",
                "pacman",
                "-Syu"
            };

        case packageManager::Apt:
            return {
                "apt",
                "install",
                "apt",
                "remove",
                "apt",
                "upgrade"
            };

        case packageManager::Dnf:
            return {
                "dnf",
                "install",
                "dnf",
                "remove",
                "dnf",
                "upgrade"
            };

        case packageManager::Zypper:
            return {
                "zypper",
                "install",
                "zypper",
                "remove",
                "zypper",
                "update"
            };

        case packageManager::Brew:
            return {
                "brew",
                "install",
                "brew",
                "uninstall",
                "brew",
                "upgrade"
            };

        case packageManager::Xbps:
            return {
                "xbps-install",
                "-S",
                "xbps-remove",
                "",
                "xbps-install",
                "-Su"
            };

        case packageManager::Unknown:
            return {};
        }

    return {};

}
