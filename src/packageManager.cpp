#include "packageManager.hpp"
#include <cstdlib>
#include <iostream>

packageManager detectPackageManager()
{
    if (std::system("command -v pacman > /dev/null 2>&1") == 0)
        return packageManager::Pacman;

    if (std::system("command -v apt > /dev/null 2>&1") == 0)
        return packageManager::Apt;

    if (std::system("command -v dnf > /dev/null 2>&1") == 0)
        return packageManager::Dnf;

    if (std::system("command -v zypper > /dev/null 2>&1") == 0)
        return packageManager::Zypper;

    if (std::system("command -v brew > /dev/null 2>&1") == 0)
        return packageManager::Brew;

    return packageManager::Unknown;
}

void initializePackageManager()
{
    switch (detectPackageManager())
    {
        case packageManager::Pacman:
            std::cout << "Pacman detected!\n";
            break;

        case packageManager::Apt:
            std::cout << "APT detected!\n";
            break;

        case packageManager::Dnf:
            std::cout << "DNF detected!\n";
            break;

        case packageManager::Zypper:
            std::cout << "Zypper detected!\n";
            break;

        case packageManager::Brew:
            std::cout << "Homebrew detected!\n";
            break;

        case packageManager::Unknown:
            std::cout << "No supported package manager detected.\n";
            break;
    }
}