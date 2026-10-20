#include "nativeInstall.hpp"
#include "shell.hpp"
#include "temporaryDirectory.hpp"

#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
using Json = nlohmann::json;

bool fail(const std::string& message)
{
    std::cerr << "Preparacao da instalacao: " << message << '\n';
    return false;
}

bool validStage(const fs::path& stage)
{
    if (geteuid() == 0)
        return fail("execute a preparacao como usuario comum.");
    if (!stage.is_absolute() || stage != stage.lexically_normal() ||
        fs::symlink_status(stage).type() != fs::file_type::directory ||
        fs::canonical(stage) != stage || !fs::is_empty(stage))
        return fail("o staging deve ser um diretorio absoluto, real e vazio.");
    return true;
}

fs::path directory(const fs::path& path)
{
    const fs::path result = fs::canonical(path);
    if (!fs::is_directory(result))
        throw std::runtime_error("diretorio de projeto invalido: " + path.string());
    return result;
}

bool within(const fs::path& path, const fs::path& root)
{
    auto p = path.begin();
    for (auto r = root.begin(); r != root.end(); ++r, ++p)
        if (p == path.end() || *p != *r) return false;
    return true;
}

bool populated(const fs::path& stage)
{
    if (fs::symlink_status(stage).type() != fs::file_type::directory ||
        fs::canonical(stage) != stage || fs::is_empty(stage))
        return fail("o instalador nao produziu um staging valido.");
    return true;
}

// Decode both ELF classes and byte orders. ET_DYN alone also describes shared
// libraries; require a program interpreter or the explicit DF_1_PIE flag.
bool elfExecutable(const fs::path& path)
{
    const auto status = fs::symlink_status(path);
    if (status.type() != fs::file_type::regular ||
        (status.permissions() & (fs::perms::owner_exec | fs::perms::group_exec |
                                 fs::perms::others_exec)) == fs::perms::none)
        return false;
    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 64> header{};
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    const auto size = fs::file_size(path);
    if (stream.gcount() < 52 || header[0] != 0x7f || header[1] != 'E' ||
        header[2] != 'L' || header[3] != 'F' || header[6] != 1 ||
        (header[4] != 1 && header[4] != 2) || (header[5] != 1 && header[5] != 2))
        return false;
    const bool wide = header[4] == 2;
    if (wide && stream.gcount() < 64) return false;
    const bool little = header[5] == 1;
    const auto number = [little](const unsigned char* bytes, std::size_t count) {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < count; ++i)
            value |= std::uint64_t(bytes[i]) << (8 * (little ? i : count - 1 - i));
        return value;
    };
    const auto type = number(header.data() + 16, 2);
    if (type != 2 && type != 3) return false;
    const auto phoff = number(header.data() + (wide ? 32 : 28), wide ? 8 : 4);
    const auto phsize = number(header.data() + (wide ? 54 : 42), 2);
    const auto phcount = number(header.data() + (wide ? 56 : 44), 2);
    if (phsize < (wide ? 56U : 32U) || phcount == 0 || phoff > size ||
        phcount > (size - phoff) / phsize)
        return false;
    bool load = false, interpreter = false, pie = false;
    for (std::uint64_t index = 0; index < phcount; ++index) {
        std::array<unsigned char, 56> ph{};
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(phoff + index * phsize));
        stream.read(reinterpret_cast<char*>(ph.data()), wide ? 56 : 32);
        if (!stream) return false;
        const auto kind = number(ph.data(), 4);
        const auto offset = number(ph.data() + (wide ? 8 : 4), wide ? 8 : 4);
        const auto length = number(ph.data() + (wide ? 32 : 16), wide ? 8 : 4);
        if (offset > size || length > size - offset) return false;
        if (kind == 1) load = true; // PT_LOAD
        if (kind == 3 && length > 1) interpreter = true; // PT_INTERP
        if (kind != 2) continue; // PT_DYNAMIC
        const std::size_t entrySize = wide ? 16 : 8;
        stream.seekg(static_cast<std::streamoff>(offset));
        for (std::uint64_t position = 0; position + entrySize <= length; position += entrySize) {
            std::array<unsigned char, 16> entry{};
            stream.read(reinterpret_cast<char*>(entry.data()), entrySize);
            if (!stream) return false;
            const auto tag = number(entry.data(), entrySize / 2);
            const auto value = number(entry.data() + entrySize / 2, entrySize / 2);
            if (tag == 0) break;
            if (tag == 0x6ffffffbU && (value & 0x08000000U)) pie = true;
        }
    }
    return load && (type == 2 || interpreter || pie);
}

void copyRegular(const fs::path& source, const fs::path& destination, fs::perms permissions)
{
    if (fs::symlink_status(source).type() != fs::file_type::regular)
        throw std::runtime_error("arquivo de instalacao nao regular: " + source.string());
    fs::create_directories(destination.parent_path());
    fs::copy_file(source, destination);
    fs::permissions(destination, permissions);
}

constexpr auto executableMode = fs::perms::owner_all | fs::perms::group_read |
    fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec;
constexpr auto documentMode = fs::perms::owner_read | fs::perms::owner_write |
    fs::perms::group_read | fs::perms::others_read;

void copyDocumentation(const fs::path& source, const std::string& name, const fs::path& stage)
{
    // Only inspect conventional names in conventional directories. Do not walk
    // a repository looking for arbitrary files that happen to be executable.
    const std::array<fs::path, 4> locations{source, source / "man", source / "doc", source / "docs"};
    std::set<std::string> copied;
    for (int section = 1; section <= 9; ++section) {
        for (const std::string& suffix : {std::string(), std::string(".gz")}) {
            const std::string filename = name + "." + std::to_string(section) + suffix;
            for (const auto& base : locations) {
                for (const auto& candidate : {base / filename, base / ("man" + std::to_string(section)) / filename}) {
                    if (fs::is_regular_file(fs::symlink_status(candidate)) && copied.insert(filename).second)
                        copyRegular(candidate, stage / "usr/share/man" / ("man" + std::to_string(section)) / filename, documentMode);
                }
            }
        }
    }
    const std::vector<std::string> filenames{name + ".md", name + ".txt", name + ".rst",
        "README", "README.md", "README.txt", "README.rst", "LICENSE", "LICENSE.md", "LICENSE.txt",
        "COPYING", "COPYING.txt", "AUTHORS", "CHANGELOG", "CHANGELOG.md"};
    copied.clear();
    for (const auto& filename : filenames)
        for (const auto& base : locations)
            if (fs::is_regular_file(fs::symlink_status(base / filename)) && copied.insert(filename).second)
                copyRegular(base / filename, stage / "usr/share/doc" / name / filename, documentMode);
}

struct MakeGraph {
    std::string goal;
    std::map<std::string, std::vector<std::string>> targets;
    std::set<std::string> phony;
};

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::vector<std::string> words(const std::string& value)
{
    std::vector<std::string> result;
    std::string word;
    bool escape = false;
    for (const char c : value) {
        if (escape) { word += c; escape = false; }
        else if (c == '\\') escape = true;
        else if (c == ' ' || c == '\t' || c == '\r') {
            if (!word.empty()) { result.push_back(word); word.clear(); }
        } else word += c;
    }
    if (escape) word += '\\';
    if (!word.empty()) result.push_back(word);
    return result;
}

MakeGraph parseMake(const std::string& database)
{
    MakeGraph graph;
    std::istringstream stream(database);
    std::string line;
    bool files = false, notTarget = false;
    while (std::getline(stream, line)) {
        if (line.rfind(".DEFAULT_GOAL :=", 0) == 0 || line.rfind(".DEFAULT_GOAL =", 0) == 0) {
            graph.goal = trim(line.substr(line.find('=') + 1));
        }
        if (line == "# Files") { files = true; continue; }
        if (line.rfind("# files hash-table stats:", 0) == 0 || line == "# VPATH Search Paths") {
            files = false; continue;
        }
        if (!files) continue;
        if (line == "# Not a target:") { notTarget = true; continue; }
        if (line.empty() || line[0] == '#' || line[0] == '\t') continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const bool skip = notTarget;
        notTarget = false;
        if (skip) continue;
        const auto names = words(line.substr(0, colon));
        auto dependencies = words(line.substr(colon + (line.compare(colon, 2, "::") == 0 ? 2 : 1)));
        // Target-specific variable assignments are not dependency rules.
        if (line.find('=', colon + 1) != std::string::npos) continue;
        for (const auto& name : names) {
            if (name.find('%') != std::string::npos) continue;
            auto& edges = graph.targets[name];
            for (const auto& dependency : dependencies)
                if (dependency != "|" && dependency != ".WAIT") edges.push_back(dependency);
        }
    }
    if (const auto it = graph.targets.find(".PHONY"); it != graph.targets.end())
        graph.phony.insert(it->second.begin(), it->second.end());
    return graph;
}

std::vector<fs::path> makeExecutables(const MakeGraph& graph, const fs::path& source)
{
    if (graph.goal.empty() || !graph.targets.contains(graph.goal))
        throw std::runtime_error("GNU make nao declarou um .DEFAULT_GOAL utilizavel.");
    std::vector<std::string> pending{graph.goal};
    std::set<std::string> visited;
    std::set<fs::path> binaries;
    while (!pending.empty()) {
        const auto name = pending.back();
        pending.pop_back();
        if (!visited.insert(name).second) continue;
        const auto target = graph.targets.find(name);
        if (target == graph.targets.end()) continue;
        pending.insert(pending.end(), target->second.begin(), target->second.end());
        if (graph.phony.contains(name)) continue;
        const auto path = (source / name).lexically_normal();
        if (!within(path, source) || !elfExecutable(path)) continue;
        const auto resolved = fs::canonical(path);
        if (!within(resolved, source))
            throw std::runtime_error("o alvo make sai do diretorio do projeto.");
        binaries.insert(resolved);
    }
    return {binaries.begin(), binaries.end()};
}

Json readJson(const fs::path& path)
{
    if (fs::symlink_status(path).type() != fs::file_type::regular)
        throw std::runtime_error("manifesto nao regular: " + path.string());
    std::ifstream stream(path);
    return Json::parse(stream);
}

bool component(const std::string& value)
{
    if (value.empty() || value == "." || value == "..") return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
    return true;
}

bool packageName(const std::string& name)
{
    if (name.empty()) return false;
    if (name[0] != '@') return component(name);
    const auto slash = name.find('/');
    return slash != std::string::npos && component(name.substr(1, slash - 1)) && component(name.substr(slash + 1));
}

std::map<std::string, fs::path> binMappings(const Json& manifest, const std::string& name)
{
    std::map<std::string, fs::path> result;
    if (!manifest.contains("bin")) return result;
    Json mappings = manifest.at("bin");
    if (mappings.is_string()) mappings = Json{{fs::path(name).filename().string(), mappings}};
    if (!mappings.is_object()) throw std::runtime_error("campo bin npm invalido.");
    for (auto it = mappings.begin(); it != mappings.end(); ++it) {
        if (!component(it.key()) || !it.value().is_string())
            throw std::runtime_error("mapeamento bin npm invalido.");
        const fs::path target = it.value().get<std::string>();
        const auto normalized = target.lexically_normal();
        if (target.empty() || target.is_absolute() || normalized == "." ||
            *normalized.begin() == ".." || target.string().find('\0') != std::string::npos)
            throw std::runtime_error("bin npm deve permanecer dentro do pacote.");
        result.emplace(it.key(), normalized);
    }
    return result;
}

Json packJson(const std::string& output)
{
    // npm can print lifecycle logs before --json output. Parse a complete JSON
    // document. npm <= 11 returns an array; npm 12 keys records by package name.
    for (std::size_t offset = 0; offset < output.size(); ++offset) {
        if ((output[offset] != '[' && output[offset] != '{') ||
            (offset && output[offset - 1] != '\n')) continue;
        Json value = Json::parse(output.substr(offset), nullptr, false);
        if (value.is_array() && value.size() == 1 && value[0].is_object()) return value[0];
        if (value.is_object() && value.size() == 1 && value.begin().value().is_object())
            return value.begin().value();
    }
    throw std::runtime_error("npm pack nao retornou metadados JSON validos.");
}

bool copyExecutables(const std::vector<fs::path>& paths, const fs::path& stage, bool docs)
{
    if (paths.empty()) return fail("nenhum executavel foi fornecido.");
    std::set<std::string> names;
    for (const auto& path : paths)
        if (!path.is_absolute() || !elfExecutable(path) || !names.insert(path.filename().string()).second)
            return fail("os executaveis devem ser arquivos ELF absolutos com nomes distintos.");
    for (const auto& path : paths) {
        copyRegular(path, stage / "usr/bin" / path.filename(), executableMode);
        if (docs) copyDocumentation(path.parent_path(), path.filename().string(), stage);
    }
    return populated(stage);
}
} // namespace

bool prepareCMake(const fs::path& buildDir, const fs::path& stage)
{
    try {
        if (!validStage(stage)) return false;
        const auto build = directory(buildDir);
        return runShell("env DESTDIR=" + quoteShell(stage.string()) + " cmake --install " +
                        quoteShell(build.string()) + " --prefix /usr").success && populated(stage);
    } catch (const std::exception& error) { return fail(error.what()); }
}

bool prepareMeson(const fs::path& buildDir, const fs::path& stage)
{
    try {
        if (!validStage(stage)) return false;
        const auto build = directory(buildDir);
        return runShell("env DESTDIR=" + quoteShell(stage.string()) + " meson install --no-rebuild -C " +
                        quoteShell(build.string())).success && populated(stage);
    } catch (const std::exception& error) { return fail(error.what()); }
}

bool prepareMake(const fs::path& sourceDir, const fs::path& stage)
{
    try {
        if (!validStage(stage)) return false;
        const auto source = directory(sourceDir);
        const std::string cd = "cd " + quoteShell(source.string()) + " && ";
        const auto metadata = runShell(cd + "(make --no-print-directory -qp; status=$?; "
            "test \"$status\" -eq 0 || test \"$status\" -eq 1)", false);
        if (!metadata.success) return fail("nao foi possivel consultar os alvos GNU make.");
        const auto graph = parseMake(metadata.output);
        if (graph.targets.contains("install"))
            return runShell(cd + "make --no-print-directory install DESTDIR=" + quoteShell(stage.string()) +
                            " prefix=/usr PREFIX=/usr").success && populated(stage);
        const auto paths = makeExecutables(graph, source);
        if (paths.size() != 1)
            return fail("sem alvo install: o grafo do alvo padrao deve conter exatamente um executavel ELF; encontrados " +
                        std::to_string(paths.size()) + ".");
        if (!copyExecutables(paths, stage, false)) return false;
        copyDocumentation(source, paths.front().filename().string(), stage);
        return true;
    } catch (const std::exception& error) { return fail(error.what()); }
}

bool prepareNpm(const fs::path& sourceDir, const fs::path& stage)
{
    try {
        if (!validStage(stage)) return false;
        const auto source = directory(sourceDir);
        const auto manifest = readJson(source / "package.json");
        const std::string name = manifest.at("name").get<std::string>();
        const std::string version = manifest.at("version").get<std::string>();
        if (!packageName(name) || version.empty()) return fail("nome ou versao npm invalido.");
        const auto bins = binMappings(manifest, name);
        TemporaryDirectory pack;
        const auto packed = runShell("cd " + quoteShell(source.string()) + " && npm pack --json --pack-destination " +
                                     quoteShell(pack.path().string()), false);
        if (!packed.success) { std::cerr << packed.output; return fail("npm pack falhou."); }
        const auto metadata = packJson(packed.output);
        const fs::path filename = metadata.at("filename").get<std::string>();
        if (filename.empty() || filename != filename.filename() || filename == "." || filename == ".." ||
            metadata.at("name") != name || metadata.at("version") != version)
            return fail("metadados inesperados no arquivo npm.");
        const auto archive = pack.path() / filename;
        if (fs::symlink_status(archive).type() != fs::file_type::regular)
            return fail("npm pack nao produziu um arquivo regular.");
        const auto prefix = stage / "usr";
        if (!runShell("npm install --global --omit=dev --no-audit --no-fund --prefix " + quoteShell(prefix.string()) +
                      " " + quoteShell(archive.string())).success) return fail("npm install falhou.");
        const auto installed = prefix / "lib/node_modules" / name;
        if (fs::symlink_status(installed).type() != fs::file_type::directory ||
            fs::canonical(installed) != installed || !within(installed, stage))
            return fail("o pacote npm instalado nao esta no staging esperado.");
        const auto actual = readJson(installed / "package.json");
        if (actual.at("name") != name || actual.at("version") != version || binMappings(actual, name) != bins ||
            actual.value("dependencies", Json::object()) != manifest.value("dependencies", Json::object()) ||
            actual.value("optionalDependencies", Json::object()) != manifest.value("optionalDependencies", Json::object()))
            return fail("o manifesto npm instalado difere do pacote solicitado.");
        for (const auto& [command, relative] : bins) {
            const auto target = fs::canonical(installed / relative);
            const auto link = prefix / "bin" / command;
            if (!within(target, installed) || !fs::is_regular_file(target) ||
                fs::symlink_status(link).type() != fs::file_type::symlink ||
                fs::canonical(link) != target || !within(fs::canonical(link), installed))
                return fail("o bin npm nao aponta para um arquivo dentro do pacote instalado: " + command);
        }
        return populated(stage);
    } catch (const std::exception& error) { return fail(error.what()); }
}

bool prepareExecutables(const std::vector<fs::path>& paths, const fs::path& stage)
{
    try {
        return validStage(stage) && copyExecutables(paths, stage, true);
    } catch (const std::exception& error) { return fail(error.what()); }
}
