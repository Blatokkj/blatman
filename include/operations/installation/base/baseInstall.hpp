#pragma once

#include <filesystem>

namespace Blatman::Install {

    namespace fs = std::filesystem;

    inline const fs::path PREFIX = "/blatman";

    inline const fs::path BIN = PREFIX / "bin";
    inline const fs::path LIB = PREFIX / "lib";
    inline const fs::path INCLUDE = PREFIX / "include";
    inline const fs::path SHARE = PREFIX / "share";

}
