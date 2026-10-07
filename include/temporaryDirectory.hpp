#pragma once

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

// Dono de um diretorio exclusivo. A limpeza ocorre tambem nos retornos de erro.
class TemporaryDirectory
{
  public:
    explicit TemporaryDirectory(
        const std::filesystem::path &parent = std::filesystem::temp_directory_path())
    {
        std::string pattern = (std::filesystem::absolute(parent) / ".blatman-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr)
            throw std::system_error(errno, std::generic_category(),
                                    "Nao foi possivel criar o diretorio temporario");
        path_ = pattern;
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path &path() const
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};
