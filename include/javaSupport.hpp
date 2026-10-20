#pragma once

#include "builderDetector.hpp"
#include <optional>
#include <string>

// Commands are relative to project; the caller controls the working directory.
std::optional<std::string> javaBuildCommand(const fs::path& project, BuildSystem system);
// The stage must be an existing, empty, absolute directory.
bool prepareJava(const fs::path& project, BuildSystem system, const fs::path& stage);
