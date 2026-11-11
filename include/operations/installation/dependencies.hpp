#pragma once

#include <functional>
#include <string>

bool resolveDependency(
    const std::string& dependency,
    const std::function<bool(const std::string&)>& installUrl = {}
);