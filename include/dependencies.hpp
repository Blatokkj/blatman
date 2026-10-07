#pragma once

#include <string>
#include <vector>

std::vector<std::string> findMissingDependencies(
    const std::string& output);

bool resolveDependency(const std::string& dependency);