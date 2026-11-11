#include "operations/installation/requirementsParser.hpp"

#include <regex>
#include <set>
#include <string>
#include <vector>

namespace
{

using PatternList = std::vector<std::regex>;

const PatternList &dependencyPatterns()
{
    static const PatternList patterns = {
        // --------------------------------------------------
        // Blatman
        // --------------------------------------------------

        std::regex(
            R"rx(blatman:missing-tool:([^\s]+))rx"),

        // --------------------------------------------------
        // Meson
        // --------------------------------------------------

        std::regex(
            R"rx((?:Run-time|Build-time|Host) dependency\s+['"]?([^'"\s]+)['"]?\s+(?:not found|found:\s*NO))rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Dependency\s+['"]([^'"]+)['"]\s+not found)rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // CMake
        // --------------------------------------------------

        std::regex(
            R"rx(Could not find a package configuration file provided by\s+"([^"]+)")rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Could NOT find\s+([A-Za-z0-9_.+-]+)\s+\(missing:)rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // pkg-config / pkgconf / Autotools
        // --------------------------------------------------

        std::regex(
            R"rx(Package\s+['"]([^'"]+)['"](?:,\s*required by\s+['"][^'"]*['"],)?\s*not found)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(No package\s+['"]([^'"]+)['"]\s+found)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Package\s+([^\s]+)\s+was not found in the pkg-config search path)rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // GCC / Clang / MSVC
        // --------------------------------------------------

        std::regex(
            R"rx(fatal error:\s*([^:'"\r\n]+):\s*No such file or directory)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(fatal error:\s*['"]([^'"]+)['"]\s+file not found)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(fatal error C1083: Cannot open include file:\s*['"]([^'"]+)['"])rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // GNU LD / Apple linker
        // --------------------------------------------------

        std::regex(
            R"rx(cannot find -l([^\s:]+))rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(library not found for -l([^\s]+))rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(ld: cannot find (lib[^\s:]+\.(?:so(?:\.[0-9]+)*|a)): No such file or directory)rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // Cargo / Rust
        // --------------------------------------------------

        std::regex(
            R"rx(no matching package named [`'"]([^`'"]+)[`'"] found)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(can't find crate for [`'"]([^`'"]+)[`'"])rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(unresolved module or unlinked crate [`'"]([^`'"]+)[`'"])rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // Go
        // --------------------------------------------------

        std::regex(
            R"rx(no required module provides package ([^\s;]+))rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(cannot find module providing package ([^\s;]+))rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // npm / Node.js
        // --------------------------------------------------

        std::regex(
            R"rx(Cannot find module ['"]([^'"]+)['"])rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Cannot find package ['"]([^'"]+)['"])rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Can't resolve ['"]([^'"]+)['"])rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // Maven / Gradle
        // --------------------------------------------------

        std::regex(
            R"rx(Could not find artifact\s+([A-Za-z0-9_.+-]+:[A-Za-z0-9_.+-]+(?::[A-Za-z0-9_.+-]+){1,2}))rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(Could not (?:find|resolve)\s+([A-Za-z0-9_.+-]+:[A-Za-z0-9_.+-]+:[A-Za-z0-9_.+-]+))rx",
            std::regex_constants::icase),

        // --------------------------------------------------
        // Python / Swift / Java
        // --------------------------------------------------

        std::regex(
            R"rx(No module named ['"]?([A-Za-z0-9_.]+)['"]?)rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(no such module ['"]([^'"]+)['"])rx",
            std::regex_constants::icase),

        std::regex(
            R"rx(package ([A-Za-z0-9_.]+) does not exist)rx",
            std::regex_constants::icase)
    };

    return patterns;
}

std::string normalizeRequirement(std::string value)
{
    // Remove pontuação que costuma acompanhar identificadores
    // nos diagnósticos.
    while (!value.empty() &&
           (value.back() == '.' ||
            value.back() == ',' ||
            value.back() == ';' ||
            value.back() == ')'))
    {
        value.pop_back();
    }

    return value;
}

void collectMatches(
    const std::string &output,
    const std::regex &pattern,
    std::set<std::string> &requirements)
{
    const std::sregex_iterator end;

    for (std::sregex_iterator match(
             output.begin(), output.end(), pattern);
         match != end; ++match)
    {
        auto requirement = normalizeRequirement((*match)[1].str());

        if (!requirement.empty())
            requirements.insert(std::move(requirement));
    }
}

// Trata mensagens do Autotools/pkg-config que apresentam
// uma lista de requisitos em vez de um único identificador.
void collectPackageRequirements(
    const std::string &output,
    std::set<std::string> &requirements)
{
    static const std::regex listPattern(
        R"rx(Package requirements\s*\(([^)]+)\)\s*were not met)rx",
        std::regex_constants::icase);

    static const std::regex itemPattern(
        R"rx(([A-Za-z_][A-Za-z0-9_+.-]*)(?:\s*(?:>=|<=|!=|==|=|>|<)\s*[0-9][A-Za-z0-9_.+-]*)?)rx");

    const std::sregex_iterator end;

    for (std::sregex_iterator list(
             output.begin(), output.end(), listPattern);
         list != end; ++list)
    {
        const std::string content = (*list)[1].str();

        for (std::sregex_iterator item(
                 content.begin(), content.end(), itemPattern);
             item != end; ++item)
        {
            auto requirement = normalizeRequirement((*item)[1].str());

            if (!requirement.empty())
                requirements.insert(std::move(requirement));
        }
    }
}

} // namespace

std::vector<std::string> findMissingRequirements(
    const std::string &output)
{
    std::set<std::string> requirements;

    for (const auto &pattern : dependencyPatterns())
        collectMatches(output, pattern, requirements);

    collectPackageRequirements(output, requirements);

    return {
        requirements.begin(),
        requirements.end()
    };
}