#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class Install
{
public:
    bool clone(
        const std::string& url,
        const fs::path& destination) const;
};