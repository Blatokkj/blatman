#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

enum class PackageManager {
    Pacman,
    Apt,
    Dnf,
    Zypper,
    Brew,
    Unknown
};

class DependencyResolver {
public:
    bool checkAndInstall(const std::string& errorMessage);
    void setLibClangPath();

private:
    std::string extractDependency(const std::string& errorMessage);
    bool installWithSystemPackageManager(const std::string& package);
    PackageManager detectPackageManager() const;
    bool installWithPacman(const std::string& package);
    bool installWithApt(const std::string& package);
    bool installWithDnf(const std::string& package);
    bool installWithZypper(const std::string& package);
    bool installWithBrew(const std::string& package);
    std::string findLibClangPath();
    std::vector<std::string> commonDependencies = {
        "glfw", "glew", "sdl2", "boost", "zlib", "openssl", "curl", "jsoncpp",
        "maven", "gradle", "npm", "nodejs", "go", "rust", "cargo"
    };
    std::vector<std::string> rustDependencies = {
        "clang", "pkgconf", "openssl", "alsa-lib", "libpulse"
    };
    std::vector<std::string> pkgConfigToPacman = {
        "glfw", "glew", "sdl2", "boost", "zlib", "openssl", "curl", "jsoncpp",
        "alsa", "pulse", "wayland", "x11", "xkbcommon", "dbus", "fontconfig", "freetype2"
    };
};