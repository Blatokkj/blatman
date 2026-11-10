#pragma once

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <iostream>

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
        if (error)
        {
            try
            {
                restoreDirectories(path_);
                std::filesystem::remove_all(path_, error);
            }
            catch (const std::exception &)
            {
            }
            if (error)
                std::cerr << "Nao foi possivel limpar temporario: " << path_ << '\n';
        }
    }

    const std::filesystem::path &path() const
    {
        return path_;
    }

  private:
    static void restoreDirectories(const std::filesystem::path &path)
    {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error || !std::filesystem::is_directory(status))
            return;
        std::filesystem::permissions(
            path, std::filesystem::perms::owner_all,
            std::filesystem::perm_options::add | std::filesystem::perm_options::nofollow, error);
        if (error)
            return;
        for (std::filesystem::directory_iterator it(path, error), end; !error && it != end;
             it.increment(error))
            restoreDirectories(it->path());
    }
    std::filesystem::path path_;
};
