#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <utility>
namespace fs = std::filesystem;
class Install
{
  public:
    explicit Install(fs::path root = "/") : destinationRoot_(std::move(root)) {}
    bool execute(const std::string &argument) const;
    std::optional<fs::path> clone(const std::string &url, const fs::path &cacheRoot) const;
    bool installPackages(const std::vector<std::string> &packages) const;
    bool publish(const fs::path &stage) const;
    const fs::path &destinationRoot() const
    {
        return destinationRoot_;
    }

  private:
    fs::path destinationRoot_;
};
