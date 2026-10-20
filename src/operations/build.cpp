#include "operations/build.hpp"
#include "javaSupport.hpp"
#include "projectWorkflow.hpp"
#include "shell.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Json = nlohmann::json;

BuildResult fail(const std::string& message, const std::string& output = {}, bool showCaptured = false)
{
    if (showCaptured)
    {
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line))
        {
            const auto item = Json::parse(line, nullptr, false);
            if (item.is_discarded() || !item.is_object()) std::cerr << line << '\n';
            else if (item.value("reason", std::string{}) == "compiler-message" &&
                     item.contains("message") && item["message"].contains("rendered") &&
                     item["message"]["rendered"].is_string())
                std::cerr << item["message"]["rendered"].get<std::string>();
        }
    }
    std::cerr << message << '\n';
    return {false, output + message + "\n", {}};
}

bool safeName(const std::string& name)
{
    return !name.empty() && name != "." && name != ".." &&
           fs::path(name).filename().string() == name && name.find('\\') == std::string::npos;
}

// go list emits a sequence of JSON objects. Ignore diagnostic lines outside JSON.
std::vector<Json> jsonObjects(const std::string& output)
{
    std::vector<Json> objects;
    std::size_t start = 0;
    while ((start = output.find('{', start)) != std::string::npos)
    {
        std::size_t end = start;
        unsigned depth = 0;
        bool quoted = false;
        bool escaped = false;
        for (; end < output.size(); ++end)
        {
            const char c = output[end];
            if (quoted)
            {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
            }
            else if (c == '"') quoted = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) break;
        }
        if (end == output.size()) throw std::runtime_error("Metadados JSON incompletos.");
        objects.push_back(Json::parse(output.substr(start, end - start + 1)));
        start = end + 1;
    }
    return objects;
}

BuildResult cargoBuild(const fs::path& source, const fs::path& destination)
{
    const std::string cd = "cd " + quoteShell(source.string()) + " && ";
    const std::string locked = fs::is_regular_file(source / "Cargo.lock") ? " --locked" : "";
    const auto metadataResult = runShell(cd + "cargo metadata --no-deps --format-version 1" + locked, false);
    if (!metadataResult.success) return fail("Falha ao consultar os metadados Cargo.", metadataResult.output, true);
    const auto objects = jsonObjects(metadataResult.output);
    if (objects.size() != 1) return fail("Metadados Cargo invalidos.", metadataResult.output);
    const Json& metadata = objects.front();
    std::set<std::string> members;
    if (metadata.contains("workspace_default_members"))
    {
        for (const auto& id : metadata.at("workspace_default_members")) members.insert(id.get<std::string>());
    }
    else
    {
        // Cargo older than workspace_default_members: resolve a non-virtual root only.
        for (const auto& package : metadata.at("packages"))
            if (fs::weakly_canonical(package.at("manifest_path").get<std::string>()) ==
                fs::weakly_canonical(source / "Cargo.toml"))
                members.insert(package.at("id").get<std::string>());
        if (members.empty())
            return fail("Cargo nao informou workspace_default_members; atualize Cargo para selecionar os binarios com seguranca.");
    }
    std::map<std::string, std::string> targets;
    std::set<std::string> requiredTargets;
    for (const auto& package : metadata.at("packages"))
    {
        const auto id = package.at("id").get<std::string>();
        if (!members.contains(id)) continue;
        for (const auto& target : package.at("targets"))
        {
            const auto kinds = target.at("kind").get<std::vector<std::string>>();
            if (std::find(kinds.begin(), kinds.end(), "bin") == kinds.end()) continue;
            const auto name = target.at("name").get<std::string>();
            if (!safeName(name)) return fail("Nome de binario Cargo invalido: " + name);
            if (!targets.emplace(name, id).second) return fail("Binarios Cargo com nomes duplicados: " + name);
            if (target.value("required-features", std::vector<std::string>{}).empty())
                requiredTargets.insert(name);
        }
    }
    const std::string buildCommand = "cargo build --release --message-format=json" +
                                     std::string(targets.empty() ? "" : " --bins") + locked;
    std::cout << "Executando: " << buildCommand << '\n' << std::flush;
    const auto built = runShell(cd + buildCommand, false);
    if (!built.success) return fail("Falha no build Cargo.", built.output, true);
    std::map<std::string, fs::path> artifacts;
    std::istringstream lines(built.output);
    std::string line;
    while (std::getline(lines, line))
    {
        const auto item = Json::parse(line, nullptr, false);
        if (item.is_discarded() || !item.is_object() ||
            item.value("reason", std::string{}) != "compiler-artifact" ||
            !item.contains("executable") || !item["executable"].is_string()) continue;
        const auto name = item.at("target").at("name").get<std::string>();
        const auto wanted = targets.find(name);
        if (wanted == targets.end() || item.value("package_id", std::string{}) != wanted->second ||
            item.at("profile").value("test", false)) continue;
        const fs::path artifact(item.at("executable").get<std::string>());
        if (!artifact.is_absolute() || !fs::is_regular_file(artifact))
            return fail("Cargo informou um executavel ausente ou nao absoluto: " + artifact.string(), built.output);
        artifacts[name] = artifact;
    }
    if (artifacts.empty() && !targets.empty())
        return fail("Cargo nao produziu executaveis para os membros selecionados.", built.output);
    for (const auto& name : requiredTargets)
        if (!artifacts.contains(name))
            return fail("Cargo nao informou o executavel obrigatorio: " + name, built.output);
    fs::create_directories(destination);
    std::vector<fs::path> executables;
    for (const auto& [name, artifact] : artifacts)
    {
        const auto target = destination / name;
        if (fs::weakly_canonical(artifact) != fs::weakly_canonical(target))
            fs::copy_file(artifact, target, fs::copy_options::overwrite_existing);
        executables.push_back(target);
    }
    return {true, built.output, executables};
}

bool excludedGoDirectory(const fs::path& relative)
{
    for (const auto& part : relative)
        if (part == "example" || part == "examples" || part == "testdata" || part == "vendor") return true;
    return false;
}

BuildResult goBuild(const fs::path& source, const fs::path& destination)
{
    const std::string cd = "cd " + quoteShell(source.string()) + " && ";
    std::vector<std::string> patterns;
    const char* selection = std::getenv("BLATMAN_GO_PACKAGES");
    const bool explicitSelection = selection && *selection;
    if (explicitSelection)
    {
        std::istringstream words(selection);
        std::string word;
        while (words >> word)
        {
            if (word.starts_with('-')) return fail("BLATMAN_GO_PACKAGES deve conter caminhos de pacotes, sem opcoes.");
            patterns.push_back(word);
        }
    }
    else
    {
        for (const auto& entry : fs::directory_iterator(source))
        {
            const auto name = entry.path().filename().string();
            if (entry.is_regular_file() && entry.path().extension() == ".go" && !name.ends_with("_test.go"))
            {
                patterns.emplace_back(".");
                break;
            }
        }
        if (fs::is_directory(source / "cmd")) patterns.emplace_back("./cmd/...");
    }
    if (patterns.empty()) return fail("Nenhum pacote Go principal encontrado; selecione com BLATMAN_GO_PACKAGES.");
    std::string command = "go list -e -json";
    for (const auto& pattern : patterns) command += " " + quoteShell(pattern);
    const auto listed = runShell(cd + command, false);
    if (!listed.success) return fail("Falha ao consultar os pacotes Go.", listed.output, true);
    std::map<std::string, std::string> packages;
    std::set<std::string> libraries;
    for (const auto& package : jsonObjects(listed.output))
    {
        const fs::path directory(package.value("Dir", std::string{}));
        const auto relative = directory.lexically_relative(source);
        if (!explicitSelection && excludedGoDirectory(relative)) continue;
        if (package.contains("Error")) return fail("Pacote Go invalido: " + package["Error"].dump(), listed.output);
        if (package.contains("DepsErrors")) return fail("Dependencias Go invalidas: " + package["DepsErrors"].dump(), listed.output);
        const auto import = package.at("ImportPath").get<std::string>();
        if (package.value("Name", std::string{}) != "main")
        {
            libraries.insert(import);
            continue;
        }
        std::string name = fs::path(import).filename().string();
        if (name.size() > 1 && name[0] == 'v' &&
            name.find_first_not_of("0123456789", 1) == std::string::npos)
            name = fs::path(import).parent_path().filename().string();
        if (!safeName(name)) return fail("Nome de executavel Go invalido: " + name);
        const auto [entry, inserted] = packages.emplace(name, import);
        if (!inserted && entry->second != import)
            return fail("Pacotes Go produzem o mesmo nome: " + name +
                        "; selecione os pacotes com BLATMAN_GO_PACKAGES.");
    }
    if (packages.empty() && libraries.empty()) return fail("Nenhum pacote Go encontrado; selecione com BLATMAN_GO_PACKAGES.", listed.output);
    fs::create_directories(destination);
    BuildResult result{true, {}, {}};
    for (const auto& import : libraries)
    {
        const auto built = runShell(cd + "go build " + quoteShell(import));
        result.output += built.output;
        if (!built.success) return fail("Falha ao compilar a biblioteca Go: " + import, result.output);
    }
    for (const auto& [name, import] : packages)
    {
        const fs::path target = destination / name;
        const auto built = runShell(cd + "go build -o " + quoteShell(target.string()) + " " + quoteShell(import));
        result.output += built.output;
        if (!built.success) return fail("Falha ao compilar o pacote Go: " + import, result.output);
        if (!fs::is_regular_file(target)) return fail("Go nao criou o executavel: " + target.string(), result.output);
        result.executables.push_back(target);
    }
    return result;
}
}

bool Build::execute(const std::string& argument) const
{
    return buildProject(argument, *this);
}

BuildResult Build::run(const fs::path& projectPath, BuildSystem system,
                       const fs::path& executableDirectory) const
{
    try
    {
        const fs::path source = fs::weakly_canonical(fs::absolute(projectPath));
        if (!fs::is_directory(source)) return fail("Diretorio do projeto nao encontrado: " + source.string());
        const fs::path destination = executableDirectory.empty()
            ? source / "build" / "blatman-bin"
            : fs::absolute(executableDirectory).lexically_normal();
        if (system == BuildSystem::Cargo) return cargoBuild(source, destination);
        if (system == BuildSystem::Go) return goBuild(source, destination);
        std::vector<std::string> commands;
        switch (system)
        {
            case BuildSystem::CMake:
                commands = {"cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_INSTALL_LIBDIR=lib", "cmake --build build"};
                break;
            case BuildSystem::Make: commands = {"make"}; break;
            case BuildSystem::Meson:
                commands = {std::string("meson setup ") +
                    (fs::is_regular_file(source / "build" / "meson-private" / "coredata.dat") ? "--reconfigure " : "") +
                    "build --prefix=/usr --bindir=bin --libdir=lib", "meson compile -C build"};
                break;
            case BuildSystem::Npm:
                commands = {fs::is_regular_file(source / "package-lock.json") || fs::is_regular_file(source / "npm-shrinkwrap.json")
                    ? "npm ci" : "npm install", "npm run build --if-present"};
                break;
            case BuildSystem::Maven:
            case BuildSystem::Gradle:
            {
                const auto command = javaBuildCommand(source, system);
                if (!command) return fail("Nao foi possivel preparar o build Java.");
                commands.push_back(*command);
                break;
            }
            case BuildSystem::Autotools:
                if (!fs::is_regular_file(source / "configure")) commands.emplace_back("autoreconf -fi");
                commands.emplace_back("sh ./configure --prefix=/usr --bindir=/usr/bin --libdir=/usr/lib");
                commands.emplace_back("make");
                break;
            default: return fail("Nenhum sistema de build reconhecido.");
        }
        BuildResult result{true, {}, {}};
        for (const auto& command : commands)
        {
            std::cout << "Executando: " << command << '\n';
            const auto executed = runShell("cd " + quoteShell(source.string()) + " && " + command);
            result.output += executed.output;
            if (!executed.success) return fail("Falha ao executar: " + command, result.output);
        }
        return result;
    }
    catch (const std::exception& error)
    {
        return fail(std::string("Falha no build: ") + error.what());
    }
}
