#pragma once

#include <filesystem>
#include <string>

namespace installation_detail
{

bool fail(const std::string& message);

bool validStage(
    const std::filesystem::path& stage
);

std::filesystem::path directory(
    const std::filesystem::path& path
);

bool within(
    const std::filesystem::path& path,
    const std::filesystem::path& root
);

bool populated(
    const std::filesystem::path& stage
);

}