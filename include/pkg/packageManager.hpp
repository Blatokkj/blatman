#pragma once
#include <string>

enum class packageManager { // classe pra representar cada gerenciador de pacotes
    Pacman,
    Apt,
    Dnf,
    Zypper,
    Brew,
    Xbps,
    Unknown
};

struct packageManagerInfo {
    std::string installCommand;
    std::string install;
    std::string removeCommand;
    std::string remove;
    std::string updateCommand;
    std::string update;
};

packageManager detectPackageManager();
void initializePackageManager();