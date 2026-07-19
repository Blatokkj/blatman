#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <optional>

namespace fs = std::filesystem;

struct PackageManifest {
    std::string name;
    std::string buildCommand;
    std::string buildSystem;
    std::vector<std::string> binaries;
    std::vector<std::string> dependencies;
    std::string repoUrl;
    std::string commitHash;
};

class Installer {
public:
    Installer(const fs::path& cacheDir, const fs::path& binDir, const fs::path& manifestDir);
    bool install(const fs::path& repoPath, const std::string& buildSystem, const std::string& repoUrl = "");
    bool addToPath() const;
    
    // New commands
    std::vector<PackageManifest> listInstalledPackages() const;
    bool upgradePackage(const std::string& packageName);
    bool upgradeAllPackages();
    size_t getPackageCount() const;
    std::optional<PackageManifest> getPackageManifest(const std::string& name) const;

private:
    fs::path cacheDir_;
    fs::path binDir_;
    fs::path manifestDir_;

    std::vector<fs::path> detectBinaries(const fs::path& repoPath, const std::string& buildSystem) const;
    bool copyBinaries(const std::vector<fs::path>& binaries);
    bool generateManifest(const fs::path& repoPath, const std::vector<fs::path>& binaries, const std::string& buildSystem, const std::string& repoUrl = "");
    std::optional<PackageManifest> parseCargoToml(const fs::path& repoPath) const;
    std::vector<fs::path> findExecutablesInDir(const fs::path& dir) const;
    std::optional<fs::path> generateMavenWrapperScript(const fs::path& repoPath, const std::string& buildSystem) const;
    std::string getCurrentCommitHash(const fs::path& repoPath) const;
};