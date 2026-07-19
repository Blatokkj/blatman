#pragma once
#include "BuildDetector.hpp"
#include "DependencyResolver.hpp"
#include "Installer.hpp"
#include <filesystem>
#include <string>
#include <fstream>

namespace fs = std::filesystem;

class Builder {
public:
    Builder();
    bool build(BuildSystem buildSystem, const fs::path& repoPath);

private:
    std::string executeCommand(const std::string& command);
    DependencyResolver resolver_;
    Installer installer_;
};