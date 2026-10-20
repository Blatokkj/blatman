#include "projectWorkflow.hpp"
#include "operations/build.hpp"
#include "operations/install.hpp"
#include "builderDetector.hpp"
#include "dependencies.hpp"
#include "nativeInstall.hpp"
#include "javaSupport.hpp"
#include "temporaryDirectory.hpp"
#include "shell.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace
{
class CacheLock
{
    int fd_;

  public:
    explicit CacheLock(const fs::path &name)
        : fd_(::open(name.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600))
    {
        if (fd_ < 0)
            throw std::runtime_error("Nao foi possivel abrir trava do cache.");
        struct stat s{};
        if (::fstat(fd_, &s) != 0 || !S_ISREG(s.st_mode) || ::flock(fd_, LOCK_EX | LOCK_NB) != 0)
        {
            ::close(fd_);
            throw std::runtime_error("Cache em uso ou trava invalida; tente novamente depois.");
        }
    }
    ~CacheLock()
    {
        ::close(fd_);
    }
    CacheLock(const CacheLock &) = delete;
    CacheLock &operator=(const CacheLock &) = delete;
};
std::vector<std::string> toolsFor(BuildSystem system, const fs::path &source)
{
    switch (system)
    {
    case BuildSystem::CMake:
        return {"cmake"};
    case BuildSystem::Make:
        return {"make"};
    case BuildSystem::Meson:
        return {"meson", "ninja"};
    case BuildSystem::Cargo:
        return {"cargo", "rustc"};
    case BuildSystem::Npm:
        return {"npm", "node"};
    case BuildSystem::Maven:
        return {fs::is_regular_file(source / "mvnw") ? "sh" : "mvn"};
    case BuildSystem::Gradle:
        return {fs::is_regular_file(source / "gradlew") ? "sh" : "gradle"};
    case BuildSystem::Autotools:
        return fs::is_regular_file(source / "configure")
                   ? std::vector<std::string>{"make"}
                   : std::vector<std::string>{"autoreconf", "make"};
    case BuildSystem::Go:
        return {"go"};
    case BuildSystem::Unknown:
        return {};
    }
    return {};
}
BuildResult checkedBuild(const Build &builder, const fs::path &source, BuildSystem system,
                         const fs::path &output)
{
    for (const auto &tool : toolsFor(system, source))
        if (!runShell("command -v " + quoteShell(tool) + " >/dev/null 2>&1", false).success)
        {
            std::cerr << "Ferramenta necessaria nao encontrada: " << tool << '\n';
            return {false, "blatman:missing-tool:" + tool, {}};
        }
    return builder.run(source, system, output);
}
} // namespace
bool buildProject(const std::string &argument, const Build &builder)
{
    if (argument.empty() || !fs::is_directory(argument))
    {
        std::cerr << "Informe um diretorio existente.\n";
        return false;
    }
    const auto source = fs::canonical(argument);
    const auto system = BuildDetector{}.detect(source);
    if (system == BuildSystem::Unknown)
        return false;
    auto result = checkedBuild(builder, source, system, {});
    if (result.success)
        std::cout << "Build concluido com sucesso!\n";
    return result.success;
}
bool installProject(const std::string &argument, const Install &installer)
{
    if (argument.empty())
    {
        std::cerr << "Informe a URL HTTPS do repositorio.\n";
        return false;
    }
    std::string identity = argument;
    while (!identity.empty() && identity.back() == '/')
        identity.pop_back();
    if (identity.ends_with(".git"))
        identity.resize(identity.size() - 4);
    static thread_local std::set<std::string> active;
    if (active.size() >= 16 || !active.insert(identity).second)
    {
        std::cerr << "Ciclo ou limite de profundidade nas dependencias por URL.\n";
        return false;
    }
    struct Guard
    {
        std::set<std::string> &set;
        std::string url;
        ~Guard()
        {
            set.erase(url);
        }
    } guard{active, identity};
    try
    {
        fs::path cacheRoot = fs::current_path() / "blatman-cache";
        fs::create_directories(cacheRoot);
        std::ostringstream lock;
        lock << ".lock-" << std::hex << std::hash<std::string>{}(identity);
        CacheLock cacheLock(cacheRoot / lock.str());
        std::cout << "Clonando: " << argument << '\n';
        const auto cloned = installer.clone(argument, cacheRoot);
        if (!cloned)
            return false;
        const auto source = fs::canonical(*cloned);
        const auto system = BuildDetector{}.detect(source, active.size() == 1);
        if (system == BuildSystem::Unknown)
            return false;
        std::cout << "Sistema detectado: " << buildSystemName(system) << '\n';
        TemporaryDirectory workspace;
        Build builder;
        BuildResult result{false, {}, {}};
        std::set<std::vector<std::string>> attempted;
        for (unsigned count = 0;; ++count)
        {
            result = checkedBuild(builder, source, system, workspace.path() / "artifacts");
            if (result.success)
                break;
            const auto missing = findMissingDependencies(result.output);
            if (missing.empty())
            {
                std::cerr << "Compilacao interrompida. Confira o diagnostico acima.\n";
                return false;
            }
            if (count >= 8 || !attempted.insert(missing).second)
            {
                std::cerr << "Requisitos continuam ausentes apos a tentativa de resolucao.\n";
                return false;
            }
            for (const auto &dependency : missing)
                if (!resolveDependency(dependency, [&](const std::string &url)
                                       { return installProject(url, installer); }))
                    return false;
        }
        std::cout << "Compilacao concluida!\n";
        const auto stage = workspace.path() / "stage";
        fs::create_directory(stage);
        bool prepared = false;
        switch (system)
        {
        case BuildSystem::CMake:
            prepared = prepareCMake(source / "build", stage);
            break;
        case BuildSystem::Meson:
            prepared = prepareMeson(source / "build", stage);
            break;
        case BuildSystem::Make:
        case BuildSystem::Autotools:
            prepared = prepareMake(source, stage);
            break;
        case BuildSystem::Npm:
            prepared = prepareNpm(source, stage);
            break;
        case BuildSystem::Cargo:
        case BuildSystem::Go:
            prepared = prepareExecutables(result.executables, stage);
            break;
        case BuildSystem::Maven:
        case BuildSystem::Gradle:
            prepared = prepareJava(source, system, stage);
            break;
        case BuildSystem::Unknown:
            return false;
        }
        if (!prepared || !installer.publish(stage))
            return false;
        std::cout << "Instalacao concluida em " << installer.destinationRoot() / "usr" << "!\n";
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Erro durante instalacao: " << e.what() << '\n';
        return false;
    }
}
