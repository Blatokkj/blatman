#include "installTransaction.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace fs = std::filesystem;
namespace
{
class Descriptor
{
    int fd_;

  public:
    explicit Descriptor(int fd) : fd_(fd)
    {
        if (fd < 0)
            throw std::runtime_error(std::strerror(errno));
    }
    ~Descriptor()
    {
        ::close(fd_);
    }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    int get() const
    {
        return fd_;
    }
};
struct Entry
{
    fs::path relative;
    fs::file_type type;
    fs::path link;
};
struct Created
{
    fs::path relative;
    dev_t device;
    ino_t inode;
    bool directory;
};
bool withinUsr(const fs::path &p)
{
    return !p.is_absolute() && !p.empty() && *p.begin() == "usr" &&
           std::none_of(p.begin(), p.end(), [](const auto &c) { return c == ".."; });
}
int openDirectory(int root, const fs::path &path)
{
    int current = ::dup(root);
    if (current < 0)
        return current;
    for (const auto &part : path)
    {
        if (part == "." || part.empty())
            continue;
        int next = ::openat(current, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        int saved = errno;
        ::close(current);
        if (next < 0)
        {
            errno = saved;
            return -1;
        }
        current = next;
    }
    return current;
}
fs::path readLink(int parent, const fs::path &name)
{
    std::array<char, 65536> buffer{};
    auto count = ::readlinkat(parent, name.c_str(), buffer.data(), buffer.size());
    if (count < 0 || static_cast<std::size_t>(count) == buffer.size())
        throw std::runtime_error("Falha lendo link: " + name.string());
    return std::string(buffer.data(), static_cast<std::size_t>(count));
}
void validateLink(const Entry &original, const std::map<fs::path, Entry> &entries)
{
    fs::path pending = original.relative;
    for (int redirects = 0; redirects < 40; ++redirects)
    {
        fs::path current;
        bool redirected = false;
        for (auto it = pending.begin(); it != pending.end(); ++it)
        {
            current /= *it;
            const auto found = entries.find(current);
            if (found == entries.end())
                throw std::runtime_error("Link pendente: " + original.relative.string());
            if (found->second.type == fs::file_type::symlink)
            {
                const auto &target = found->second.link;
                fs::path next =
                    target.is_absolute() ? target.relative_path() : current.parent_path() / target;
                for (++it; it != pending.end(); ++it)
                    next /= *it;
                pending = next.lexically_normal();
                if (pending.filename().empty())
                    pending = pending.parent_path();
                if (!withinUsr(pending))
                    throw std::runtime_error("Link fora de /usr: " + original.relative.string());
                redirected = true;
                break;
            }
            if (std::next(it) != pending.end() && found->second.type != fs::file_type::directory)
                throw std::runtime_error("Link atravessa arquivo: " + original.relative.string());
        }
        if (!redirected)
            return;
    }
    throw std::runtime_error("Ciclo de links: " + original.relative.string());
}
void preflight(int root, const Entry &entry)
{
    fs::path current;
    for (const auto &part : entry.relative)
    {
        Descriptor parent(openDirectory(root, current));
        struct stat status{};
        if (::fstatat(parent.get(), part.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0)
        {
            if (errno == ENOENT)
                return;
            throw std::runtime_error("Falha verificando destino: " + entry.relative.string());
        }
        current /= part;
        bool directory = current != entry.relative || entry.type == fs::file_type::directory;
        if (!directory || !S_ISDIR(status.st_mode))
            throw std::runtime_error("Conflito no destino: " + current.string());
    }
}
void recordCreated(int parent, const fs::path &path, bool directory, std::vector<Created> &created)
{
    struct stat s{};
    if (::fstatat(parent, path.filename().c_str(), &s, AT_SYMLINK_NOFOLLOW) != 0)
        throw std::runtime_error("Falha registrando destino: " + path.string());
    created.push_back({path, s.st_dev, s.st_ino, directory});
}
void rollback(int root, const std::vector<Created> &created)
{
    for (auto it = created.rbegin(); it != created.rend(); ++it)
    {
        int fd = openDirectory(root, it->relative.parent_path());
        if (fd < 0)
        {
            std::cerr << "Rollback: confira " << it->relative << '\n';
            continue;
        }
        Descriptor parent(fd);
        struct stat now{};
        const auto name = it->relative.filename();
        if (::fstatat(fd, name.c_str(), &now, AT_SYMLINK_NOFOLLOW) != 0)
        {
            if (errno != ENOENT)
                std::cerr << "Rollback: confira " << it->relative << '\n';
            continue;
        }
        // Nao remove um inode que ja foi substituido. Escritores externos
        // devem respeitar a trava para nao disputar a verificacao/remocao.
        if (now.st_dev != it->device || now.st_ino != it->inode ||
            ::unlinkat(fd, name.c_str(), it->directory ? AT_REMOVEDIR : 0) != 0)
            std::cerr << "Rollback incompleto: confira " << it->relative << '\n';
    }
}
} // namespace
bool publishTree(const fs::path &stagePath, const fs::path &rootPath)
{
    std::vector<Created> created;
    try
    {
        if (!rootPath.is_absolute())
            throw std::runtime_error("Destino precisa ser absoluto.");
        const auto stage = fs::canonical(stagePath);
        Descriptor source(::open(stage.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        Descriptor root(::open(rootPath.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        if (::flock(root.get(), LOCK_EX | LOCK_NB) != 0)
            throw std::runtime_error("Outra instalacao esta publicando neste destino.");
        std::map<fs::path, Entry> entries;
        std::size_t files = 0;
        for (const auto &item : fs::recursive_directory_iterator(stage))
        {
            const auto relative = item.path().lexically_relative(stage);
            const auto status = item.symlink_status();
            const auto type = status.type();
            if (!withinUsr(relative))
                throw std::runtime_error("Receita tentou instalar fora de /usr: " +
                                         relative.string());
            if (type != fs::file_type::directory && type != fs::file_type::regular &&
                type != fs::file_type::symlink)
                throw std::runtime_error("Tipo nao suportado: " + relative.string());
            if (type != fs::file_type::symlink &&
                (status.permissions() & (fs::perms::set_uid | fs::perms::set_gid)) !=
                    fs::perms::none)
                throw std::runtime_error("Permissoes especiais nao suportadas: " +
                                         relative.string());
            entries.emplace(
                relative,
                Entry{relative, type,
                      type == fs::file_type::symlink ? fs::read_symlink(item.path()) : fs::path{}});
            files += type != fs::file_type::directory;
        }
        if (!files)
            throw std::runtime_error("A receita nao preparou arquivos para instalar.");
        for (const auto &[name, entry] : entries)
        {
            (void)name;
            if (entry.type == fs::file_type::symlink)
                validateLink(entry, entries);
            preflight(root.get(), entry);
        }
        created.reserve(entries.size());
        try
        {
            for (const auto &[relative, entry] : entries)
            {
                Descriptor parent(openDirectory(root.get(), relative.parent_path()));
                const auto name = relative.filename();
                if (entry.type == fs::file_type::directory)
                {
                    if (::mkdirat(parent.get(), name.c_str(), 0755) == 0)
                        recordCreated(parent.get(), relative, true, created);
                    else if (errno != EEXIST)
                        throw std::runtime_error("Falha criando diretorio: " + relative.string());
                    Descriptor directory(::openat(parent.get(), name.c_str(),
                                                  O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    continue;
                }
                Descriptor sourceParent(openDirectory(source.get(), relative.parent_path()));
                if (entry.type == fs::file_type::symlink)
                {
                    if (readLink(sourceParent.get(), name) != entry.link)
                        throw std::runtime_error("Link mudou durante instalacao: " +
                                                 relative.string());
                    if (::symlinkat(entry.link.c_str(), parent.get(), name.c_str()) != 0)
                        throw std::runtime_error("Falha criando link: " + relative.string());
                    recordCreated(parent.get(), relative, false, created);
                    continue;
                }
                Descriptor input(::openat(sourceParent.get(), name.c_str(),
                                          O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
                struct stat s{};
                if (::fstat(input.get(), &s) != 0 || !S_ISREG(s.st_mode) ||
                    (s.st_mode & (S_ISUID | S_ISGID)))
                    throw std::runtime_error("Arquivo mudou durante instalacao: " +
                                             relative.string());
                Descriptor output(::openat(parent.get(), name.c_str(),
                                           O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                                           0600));
                struct stat copied{};
                if (::fstat(output.get(), &copied) != 0)
                    throw std::runtime_error("Falha registrando copia.");
                created.push_back({relative, copied.st_dev, copied.st_ino, false});
                std::array<char, 65536> buffer{};
                while (true)
                {
                    auto count = ::read(input.get(), buffer.data(), buffer.size());
                    if (count == 0)
                        break;
                    if (count < 0)
                    {
                        if (errno == EINTR)
                            continue;
                        throw std::runtime_error("Falha lendo " + relative.string());
                    }
                    ssize_t offset = 0;
                    while (offset < count)
                    {
                        auto written = ::write(output.get(), buffer.data() + offset,
                                               static_cast<std::size_t>(count - offset));
                        if (written < 0 && errno == EINTR)
                            continue;
                        if (written <= 0)
                            throw std::runtime_error("Falha gravando " + relative.string());
                        offset += written;
                    }
                }
                // Nunca publica binarios gravaveis por grupo/outros.
                if (::fchmod(output.get(), s.st_mode & 0755) != 0 || ::fsync(output.get()) != 0)
                    throw std::runtime_error("Falha finalizando " + relative.string());
            }
        }
        catch (...)
        {
            rollback(root.get(), created);
            throw;
        }
        return true;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Instalacao interrompida: " << error.what() << '\n';
        return false;
    }
}
