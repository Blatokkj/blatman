#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class Install
{
public:
    std::optional<fs::path> clone(
        const std::string& url,
        const fs::path& cacheRoot) const;
    bool installExecutable(
    const fs::path& repositoryPath,
    const fs::path& relativeExecutable) const;
    bool installPackages(
    const std::vector<std::string>& packages) const;
    bool installCMake(const fs::path& buildDirectory) const;
};