#pragma once
#include <string>

enum class packageManager { // classe pra representar cada gerenciador de pacotes
    Pacman,
    Apt,
    Dnf,
    Zypper,
    Brew,
    Unknown
};

struct packageManagerInfo { // a base dos comandos, instalar, remover, atualizar...
    std::string command;
    std::string install;
    std::string remove;
    std::string update;
};

packageManager detectPackageManager();
void initializePackageManager();