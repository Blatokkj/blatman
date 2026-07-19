#include "DependencyResolver.hpp"
#include "Logger.hpp"
#include <regex>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <fstream>
#include <unistd.h>

bool DependencyResolver::checkAndInstall(const std::string& errorMessage) {
    std::string package = extractDependency(errorMessage);
    if (package.empty()) {
        return false;
    }

    Logger logger(fs::path(getenv("HOME")) / ".blatman" / "logs");
    logger.logInfo("Tentando resolver dependencia: " + package);

    PackageManager pm = detectPackageManager();
    if (pm == PackageManager::Unknown) {
        logger.logError("Gerenciador de pacotes nao suportado.");
        return false;
    }

    bool success = false;
    switch (pm) {
        case PackageManager::Pacman:
            success = installWithPacman(package);
            break;
        case PackageManager::Apt:
            success = installWithApt(package);
            break;
        case PackageManager::Dnf:
            success = installWithDnf(package);
            break;
        case PackageManager::Zypper:
            success = installWithZypper(package);
            break;
        case PackageManager::Brew:
            success = installWithBrew(package);
            break;
        default:
            return false;
    }

    if (success) {
        logger.logInfo("Dependencia instalada: " + package);
        if (package == "clang") {
            setLibClangPath();
        }
        return true;
    }

    logger.logError("Falha ao instalar dependencia: " + package);
    return false;
}

PackageManager DependencyResolver::detectPackageManager() const {
    if (fs::exists("/usr/bin/pacman") || fs::exists("/bin/pacman")) {
        return PackageManager::Pacman;
    }
    if (fs::exists("/usr/bin/apt") || fs::exists("/bin/apt")) {
        return PackageManager::Apt;
    }
    if (fs::exists("/usr/bin/dnf") || fs::exists("/bin/dnf")) {
        return PackageManager::Dnf;
    }
    if (fs::exists("/usr/bin/zypper") || fs::exists("/bin/zypper")) {
        return PackageManager::Zypper;
    }
    if (fs::exists("/usr/local/bin/brew") || fs::exists("/opt/homebrew/bin/brew") || fs::exists("/home/linuxbrew/.linuxbrew/bin/brew")) {
        return PackageManager::Brew;
    }
    return PackageManager::Unknown;
}

std::string DependencyResolver::extractDependency(const std::string& errorMessage) {
    std::regex clangPattern(R"((Unable to find libclang: .*libclang[^']+))");
    if (std::regex_search(errorMessage, clangPattern)) {
        return "clang";
    }

    std::regex pkgConfigPattern(R"((Package ([A-Za-z0-9_-]+) was not found))");
    std::smatch pkgMatches;
    if (std::regex_search(errorMessage, pkgMatches, pkgConfigPattern) && pkgMatches.size() > 2) {
        std::string package = pkgMatches[2].str();
        if (std::find(commonDependencies.begin(), commonDependencies.end(), package) != commonDependencies.end() ||
            std::find(rustDependencies.begin(), rustDependencies.end(), package) != rustDependencies.end()) {
            return package;
        }
    }

    std::regex pattern(R"((fatal error: |cannot find )([A-Za-z0-9_/-]+)\.h(pp)?:)");
    std::smatch matches;
    if (std::regex_search(errorMessage, matches, pattern) && matches.size() > 2) {
        std::string dependency = matches[2].str();
        size_t lastSlash = dependency.find_last_of("/");
        if (lastSlash != std::string::npos) {
            dependency = dependency.substr(lastSlash + 1);
        }
        dependency.erase(std::remove_if(dependency.begin(), dependency.end(), ::isdigit), dependency.end());
        if (std::find(commonDependencies.begin(), commonDependencies.end(), dependency) != commonDependencies.end() ||
            std::find(rustDependencies.begin(), rustDependencies.end(), dependency) != rustDependencies.end()) {
            return dependency;
        }
    }
    return "";
}

std::string DependencyResolver::findLibClangPath() {
    std::string command = "find /usr -name \"libclang.so*\" 2>/dev/null | head -n 1 | xargs dirname";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return "";
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result.empty() ? "" : result.substr(0, result.size() - 1);
}

void DependencyResolver::setLibClangPath() {
    std::string libClangPath = findLibClangPath();
    if (!libClangPath.empty()) {
        setenv("LIBCLANG_PATH", libClangPath.c_str(), 1);
    }
}

bool DependencyResolver::installWithPacman(const std::string& package) {
    std::string mappedPackage = package;
    if (std::find(pkgConfigToPacman.begin(), pkgConfigToPacman.end(), package) != pkgConfigToPacman.end()) {
        mappedPackage = package;
    }
    // Map common build tool names to pacman packages
    if (package == "maven") mappedPackage = "maven";
    else if (package == "gradle") mappedPackage = "gradle";
    else if (package == "npm" || package == "nodejs") mappedPackage = "nodejs npm";
    else if (package == "go") mappedPackage = "go";
    else if (package == "rust" || package == "cargo") mappedPackage = "rust";
    std::string command = "sudo pacman -S --noconfirm " + mappedPackage + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool DependencyResolver::installWithApt(const std::string& package) {
    std::string mappedPackage = package;
    if (package == "maven") mappedPackage = "maven";
    else if (package == "gradle") mappedPackage = "gradle";
    else if (package == "npm" || package == "nodejs") mappedPackage = "nodejs npm";
    else if (package == "go") mappedPackage = "golang-go";
    else if (package == "rust" || package == "cargo") mappedPackage = "rustc cargo";
    std::string command = "sudo apt update && sudo apt install -y " + mappedPackage + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool DependencyResolver::installWithDnf(const std::string& package) {
    std::string mappedPackage = package;
    if (package == "maven") mappedPackage = "maven";
    else if (package == "gradle") mappedPackage = "gradle";
    else if (package == "npm" || package == "nodejs") mappedPackage = "nodejs npm";
    else if (package == "go") mappedPackage = "golang";
    else if (package == "rust" || package == "cargo") mappedPackage = "rust cargo";
    std::string command = "sudo dnf install -y " + mappedPackage + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool DependencyResolver::installWithZypper(const std::string& package) {
    std::string mappedPackage = package;
    if (package == "maven") mappedPackage = "maven";
    else if (package == "gradle") mappedPackage = "gradle";
    else if (package == "npm" || package == "nodejs") mappedPackage = "nodejs npm";
    else if (package == "go") mappedPackage = "go";
    else if (package == "rust" || package == "cargo") mappedPackage = "rust cargo";
    std::string command = "sudo zypper install -y " + mappedPackage + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}

bool DependencyResolver::installWithBrew(const std::string& package) {
    std::string brewPath;
    if (fs::exists("/usr/local/bin/brew")) brewPath = "/usr/local/bin/brew";
    else if (fs::exists("/opt/homebrew/bin/brew")) brewPath = "/opt/homebrew/bin/brew";
    else if (fs::exists("/home/linuxbrew/.linuxbrew/bin/brew")) brewPath = "/home/linuxbrew/.linuxbrew/bin/brew";
    else return false;
    
    std::string mappedPackage = package;
    if (package == "maven") mappedPackage = "maven";
    else if (package == "gradle") mappedPackage = "gradle";
    else if (package == "npm" || package == "nodejs") mappedPackage = "node";
    else if (package == "go") mappedPackage = "go";
    else if (package == "rust" || package == "cargo") mappedPackage = "rust";
    
    std::string command = brewPath + " install " + mappedPackage + " 2>&1";
    int result = system(command.c_str());
    return result == 0;
}