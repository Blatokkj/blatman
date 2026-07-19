#pragma once
#include <string>
#include <vector>
#include <stdexcept>

enum class Command {
    Install,
    List,
    Upgrade,
    Packs,
    Help,
    Unknown
};

class ArgumentParser {
public:
    ArgumentParser(int argc, char* argv[]);
    Command getCommand() const;
    std::string getUrl() const;
    std::string getPackageName() const;

private:
    Command command_;
    std::string url_;
    std::string packageName_;
};