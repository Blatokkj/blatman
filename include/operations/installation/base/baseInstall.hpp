#pragma once

#include <filesystem>

namespace Blatman::Install {

    namespace fs = std::filesystem;

    struct InstallPaths {
        fs::path homePath;
        fs::path PREFIX;
        fs::path BIN;
        fs::path LIB;
        fs::path INCLUDE;
        fs::path SHARE;
    };

    const InstallPaths& getInstallPaths();

}