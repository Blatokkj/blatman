#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

class Install
{
public:
    std::optional<fs::path> clone(
        const std::string& url,
        const fs::path& cacheRoot) const;
};