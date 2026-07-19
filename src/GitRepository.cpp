#include "GitRepository.hpp"
#include <cstdlib>
#include <iostream>
#include <array>
#include <memory>
#include <regex>

bool GitRepository::clone(const std::string& url, const fs::path& dest) {
    if (fs::exists(dest)) {
        fs::remove_all(dest);
    }
    fs::create_directories(dest);

    if (!isValidGitHubUrl(url)) {
        return false;
    }

    std::string command = "git clone --depth 1 " + escapeShellArg(url) + " " + escapeShellArg(dest.string()) + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool GitRepository::pull(const fs::path& repoPath) {
    if (!fs::exists(repoPath) || !fs::exists(repoPath / ".git")) {
        return false;
    }

    std::string command = "cd " + escapeShellArg(repoPath.string()) + " && git fetch origin && git reset --hard origin/HEAD 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool GitRepository::isValidGitHubUrl(const std::string& url) {
    std::regex githubPattern(R"(^https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(\.git)?/?$)");
    return std::regex_match(url, githubPattern);
}

std::string GitRepository::escapeShellArg(const std::string& arg) {
    std::string escaped = "'";
    for (char c : arg) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}