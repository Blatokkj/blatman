#pragma once

#include <string>
#include <vector>
#include <functional>

std::vector<std::string> findMissingDependencies(
    const std::string& output);

bool resolveDependency(const std::string& dependency, const std::function<bool(const std::string&)>& installUrl = {});