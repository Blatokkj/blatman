#pragma once

#include <filesystem>

namespace mozilla_detail
{
    namespace fs = std::filesystem;

    struct Environment
    {
        fs::path source;
        fs::path object;
    };

    struct BuildLayout
    {
        fs::path distributionDirectory;
    };

    Environment inspectMozilla(const fs::path& source);

    BuildLayout inspectBuildLayout(const Environment& environment);

    fs::path packageMozilla(
        const fs::path& source,
        const BuildLayout& layout
    );

    bool inspectInstallMozilla(const Environment& environment);

    bool stageMozilla(
        const Environment& environment,
        const fs::path& stage,
        const fs::path& prefix
    );

    bool validateMozilla(
        const fs::path& stage,
        const fs::path& prefix
    );
}