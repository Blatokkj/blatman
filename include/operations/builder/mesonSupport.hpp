#pragma once

#include "operations/builder/build.hpp"

#include <filesystem>

BuildResult buildMesonProject(
    const std::filesystem::path& source
);