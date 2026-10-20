#pragma once

#include <filesystem>
#include <vector>

// Prepare an installation without publishing it. The caller owns an existing,
// empty, absolute staging directory. These helpers refuse privileged execution.
bool prepareCMake(const std::filesystem::path& buildDir, const std::filesystem::path& stage);
bool prepareMeson(const std::filesystem::path& buildDir, const std::filesystem::path& stage);
bool prepareMake(const std::filesystem::path& source, const std::filesystem::path& stage);
bool prepareNpm(const std::filesystem::path& source, const std::filesystem::path& stage);
bool prepareExecutables(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& stage);
