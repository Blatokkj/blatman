#pragma once
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class GitRepository {
public:
    bool clone(const std::string& url, const fs::path& dest);
    bool pull(const fs::path& repoPath);

private:
    static bool isValidGitHubUrl(const std::string& url);
    static std::string escapeShellArg(const std::string& arg);
};