#include "ArgumentParser.hpp"
#include <regex>
#include <iostream>

ArgumentParser::ArgumentParser(int argc, char* argv[]) {
    if (argc < 2) {
        command_ = Command::Help;
        return;
    }

    std::string cmd = argv[1];

    if (cmd == "install" || cmd == "--url") {
        command_ = Command::Install;
        if (argc != 3) {
            throw std::invalid_argument("Uso: blatman install <URL-do-GitHub>");
        }
        url_ = argv[2];
        std::regex githubPattern(R"(^https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(\.git)?/?$)");
        if (!std::regex_match(url_, githubPattern)) {
            throw std::invalid_argument("URL invalida. Use: https://github.com/usuario/repositorio");
        }
    } else if (cmd == "list") {
        command_ = Command::List;
    } else if (cmd == "upgrade") {
        command_ = Command::Upgrade;
        if (argc == 3) {
            packageName_ = argv[2];
        }
    } else if (cmd == "packs") {
        command_ = Command::Packs;
    } else if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        command_ = Command::Help;
    } else {
        command_ = Command::Unknown;
    }
}

Command ArgumentParser::getCommand() const {
    return command_;
}

std::string ArgumentParser::getUrl() const {
    return url_;
}

std::string ArgumentParser::getPackageName() const {
    return packageName_;
}