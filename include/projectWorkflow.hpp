#pragma once
#include <string>
class Build;
class Install;
bool buildProject(const std::string &, const Build &);
bool installProject(const std::string &, const Install &);
