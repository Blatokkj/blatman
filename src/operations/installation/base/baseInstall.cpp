
#include "operations/installation/base/baseInstall.hpp"

#include <pwd.h>
#include <unistd.h>
#include <stdexcept>

namespace Blatman::Install {

    const InstallPaths& getInstallPaths() {

        static const InstallPaths paths = [] {

            struct passwd* user = getpwuid(getuid());

            if (user == nullptr || user->pw_dir == nullptr) {
                throw std::runtime_error(
                    "Não foi possível identificar o diretório do usuário"
                );
            }

            fs::path homePath = user->pw_dir;
            fs::path prefix = homePath / ".blatman";

            return InstallPaths{
                homePath,
                prefix,
                prefix / "bin",
                prefix / "lib",
                prefix / "include",
                prefix / "share"
            };

        }();

        return paths;
    }

}
