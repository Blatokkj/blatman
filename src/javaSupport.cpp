#include "javaSupport.hpp"
#include "shell.hpp"
#include "temporaryDirectory.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace {
struct Version { int major = 0; int minor = 0; };
struct Java { fs::path home; int major = 0; };

std::string readFile(const fs::path& path)
{
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::optional<Version> gradleVersion(const fs::path& project)
{
    const auto properties = readFile(project / "gradle/wrapper/gradle-wrapper.properties");
    std::smatch match;
    if (std::regex_search(properties, match, std::regex(R"(gradle-([0-9]+)\.([0-9]+)(?:\.[0-9]+)?-(?:bin|all)\.zip)")))
        return Version{std::stoi(match[1]), std::stoi(match[2])};
    if (fs::is_regular_file(project / "gradlew")) {
        std::cerr << "Cannot determine Gradle wrapper version from gradle/wrapper/gradle-wrapper.properties.\n";
        return std::nullopt;
    }
    const auto result = runShell("gradle --version", false);
    if (result.success && std::regex_search(result.output, match, std::regex(R"(Gradle ([0-9]+)\.([0-9]+))")))
        return Version{std::stoi(match[1]), std::stoi(match[2])};
    std::cerr << "Cannot determine installed Gradle version; use a versioned Gradle wrapper.\n";
    return std::nullopt;
}

std::pair<int, int> javaRange(Version version)
{
    const auto atLeast = [version](int major, int minor) {
        return std::pair{version.major, version.minor} >= std::pair{major, minor};
    };
    const int minimum = version.major >= 9 ? 17 : 8;
    int maximum = 8;
    for (const auto& entry : std::vector<std::pair<Version, int>>{
             {{4, 3}, 9}, {{4, 7}, 10}, {{5, 0}, 11}, {{5, 4}, 12},
             {{6, 0}, 13}, {{6, 3}, 14}, {{6, 7}, 15}, {{7, 0}, 16},
             {{7, 3}, 17}, {{7, 5}, 18}, {{7, 6}, 19}, {{8, 3}, 20},
             {{8, 5}, 21}, {{8, 8}, 22}, {{8, 10}, 23}, {{8, 14}, 24},
             {{9, 1}, 25}, {{9, 4}, 26}, {{9, 8}, 27}}) {
        if (atLeast(entry.first.major, entry.first.minor)) maximum = entry.second;
    }
    return {minimum, maximum};
}

std::optional<Java> inspectJava(const fs::path& candidate)
{
    std::error_code error;
    const auto home = fs::canonical(candidate, error);
    if (error || !fs::is_regular_file(home / "bin/java") || !fs::is_regular_file(home / "bin/javac"))
        return std::nullopt;
    std::smatch match;
    const auto release = readFile(home / "release");
    if (std::regex_search(release, match, std::regex(R"(JAVA_VERSION="(?:1\.)?([0-9]+))")))
        return Java{home, std::stoi(match[1])};
    const auto result = runShell(quoteShell((home / "bin/java").string()) + " -version", false);
    if (result.success && std::regex_search(result.output, match, std::regex(R"(version "(?:1\.)?([0-9]+))")))
        return Java{home, std::stoi(match[1])};
    return std::nullopt;
}

std::optional<Java> selectJava(const fs::path& project, BuildSystem system)
{
    std::pair<int, int> range{8, 1000};
    std::optional<Version> gradle;
    if (system == BuildSystem::Gradle) {
        gradle = gradleVersion(project);
        if (!gradle) return std::nullopt;
        range = javaRange(*gradle);
    }
    const auto compatible = [range](const Java& java) {
        return java.major >= range.first && java.major <= range.second;
    };
    if (const char* overrideHome = std::getenv("BLATMAN_JAVA_HOME")) {
        const auto java = inspectJava(overrideHome);
        if (java && compatible(*java)) return java;
        std::cerr << "BLATMAN_JAVA_HOME must identify a compatible JDK (Java "
                  << range.first << ".." << range.second << ").\n";
        return std::nullopt;
    }
    std::vector<fs::path> homes;
    if (const char* home = std::getenv("JAVA_HOME")) homes.emplace_back(home);
    if (const char* path = std::getenv("PATH")) {
        std::istringstream entries(path);
        std::string entry;
        while (std::getline(entries, entry, ':')) {
            std::error_code error;
            const auto java = fs::canonical(fs::path(entry.empty() ? "." : entry) / "java", error);
            if (!error) homes.push_back(java.parent_path().parent_path());
        }
    }
    std::error_code error;
    std::vector<fs::path> installed;
    for (fs::directory_iterator it("/usr/lib/jvm", error), end; !error && it != end; it.increment(error))
        installed.push_back(it->path());
    std::sort(installed.begin(), installed.end());
    homes.insert(homes.end(), installed.begin(), installed.end());
    std::set<fs::path> seen;
    for (const auto& home : homes) {
        const auto java = inspectJava(home);
        if (java && seen.insert(java->home).second && compatible(*java)) return java;
    }
    std::cerr << "No compatible installed JDK found";
    if (gradle) std::cerr << " for Gradle " << gradle->major << '.' << gradle->minor;
    std::cerr << ". Need Java " << range.first << ".." << range.second
              << "; select an installed JDK using BLATMAN_JAVA_HOME";
    if (range.first <= 17 && range.second >= 17) std::cerr << " (JDK 17 is compatible)";
    std::cerr << ". No JDK was downloaded or changed.\n";
    return std::nullopt;
}

std::string prefix(const Java& java)
{
    const char* path = std::getenv("PATH");
    return "env JAVA_HOME=" + quoteShell(java.home.string()) + " PATH=" +
           quoteShell((java.home / "bin").string() + ":" + (path ? path : "/usr/bin:/bin")) + " ";
}

std::string executable(const fs::path& project, BuildSystem system)
{
    if (system == BuildSystem::Gradle)
        return fs::is_regular_file(project / "gradlew") ? "sh ./gradlew" : "gradle";
    return fs::is_regular_file(project / "mvnw") ? "sh ./mvnw" : "mvn";
}

std::string gradleOptions(const Java& java)
{
    return " --no-daemon --console=plain -Porg.gradle.java.installations.auto-download=false " +
           quoteShell("-Dorg.gradle.java.home=" + java.home.string());
}

// Stage only explicitly selected local publishing tasks and application distributions.
const char* gradleScript = R"BLATMAN(
import org.gradle.api.publish.maven.MavenPublication
import org.gradle.api.publish.maven.tasks.PublishToMavenRepository
import org.gradle.api.publish.maven.tasks.PublishToMavenLocal
import org.gradle.api.publish.ivy.tasks.PublishToIvyRepository
import org.gradle.api.tasks.Sync

def stagePath = System.getProperty('blatman.stage')
if (!stagePath) throw new GradleException('Missing blatman.stage')
def stage = new File(stagePath).canonicalFile
def repository = new File(stage, 'usr/share/maven-repo')
def repoUri = repository.toURI()
def selected = []
def coordinates = [:]
def apps = [:]
def allowed = [] as Set

gradle.beforeProject { p ->
    p.pluginManager.withPlugin('java-library') {
        p.pluginManager.apply('maven-publish')
    }
}
gradle.projectsEvaluated {
    gradle.rootProject.allprojects.each { p ->
        if (p.plugins.hasPlugin('maven-publish')) {
            def publishing = p.extensions.getByName('publishing')
            if (publishing.publications.empty && p.plugins.hasPlugin('java-library') &&
                p.components.findByName('java') != null && p.group.toString() &&
                p.version.toString() && p.version.toString() != 'unspecified') {
                publishing.publications.create('blatmanLibrary', MavenPublication) {
                    from p.components.getByName('java')
                }
            }
            def pubs = publishing.publications.withType(MavenPublication).toList()
            pubs.each { pub ->
                def parts = [pub.groupId, pub.artifactId, pub.version]
                if (parts.any { !it || it == 'unspecified' || !(it ==~ /[A-Za-z0-9_][A-Za-z0-9_.+-]*/) || it.contains('..') })
                    throw new GradleException("Invalid Maven coordinates for ${p.path}:${pub.name}")
                def key = parts.join(':')
                if (coordinates.containsKey(key))
                    throw new GradleException("Duplicate Maven coordinates ${key}: ${coordinates[key]} and ${p.path}:${pub.name}")
                coordinates[key] = "${p.path}:${pub.name}"
            }
            if (!pubs.empty) {
                if (publishing.repositories.findByName('blatmanStage') != null)
                    throw new GradleException('Repository name blatmanStage is reserved')
                publishing.repositories.maven { name = 'blatmanStage'; url = repoUri }
                p.tasks.withType(PublishToMavenRepository).each { task ->
                    if (task.repository.name == 'blatmanStage' && pubs.contains(task.publication)) {
                        selected << task
                        allowed << task.path
                    }
                }
            }
        }
        if (p.plugins.hasPlugin('application')) {
            def name = p.extensions.getByName('application').applicationName
            if (!name || !(name ==~ /[A-Za-z0-9_][A-Za-z0-9_.+-]*/) || name.contains('..'))
                throw new GradleException("Unsafe application name: ${name}")
            if (apps.containsKey(name)) throw new GradleException("Duplicate application name: ${name}")
            apps[name] = p.path
            def destination = new File(stage, "usr/lib/blatman/${name}")
            def install = p.tasks.named('installDist', Sync).get()
            install.into(destination)
            install.doLast {
                def bin = new File(stage, 'usr/bin')
                bin.mkdirs()
                def scripts = new File(destination, 'bin').listFiles()?.findAll { it.isFile() && !it.name.endsWith('.bat') } ?: []
                if (scripts.empty) throw new GradleException("No launchers in application distribution: ${name}")
                scripts.each { script ->
                    if (!(script.name ==~ /[A-Za-z0-9_][A-Za-z0-9_.+-]*/) || script.name.contains('..'))
                        throw new GradleException("Unsafe launcher name: ${script.name}")
                    def launcher = new File(bin, script.name)
                    if (launcher.exists()) throw new GradleException("Duplicate launcher: ${script.name}")
                    launcher.text = '#!/bin/sh\nexec /usr/lib/blatman/' + name + '/bin/' + script.name + ' "$@"\n'
                    if (!launcher.setExecutable(true, false)) throw new GradleException("Cannot make launcher executable: ${launcher}")
                }
            }
            selected << install
        }
    }
    if (selected.empty) throw new GradleException('No Maven publications, publishable java-library components, or application distributions found')
    if (gradle.rootProject.tasks.findByName('blatmanStageInstall') != null)
        throw new GradleException('Task name blatmanStageInstall is reserved')
    gradle.rootProject.tasks.register('blatmanStageInstall') { dependsOn selected }
}
gradle.taskGraph.whenReady { graph ->
    graph.allTasks.each { task ->
        if (task instanceof PublishToMavenLocal)
            throw new GradleException("Refusing user Maven-local publication: ${task.path}")
        if (task instanceof PublishToIvyRepository)
            throw new GradleException("Refusing Ivy publication: ${task.path}")
        if (task instanceof PublishToMavenRepository && (!allowed.contains(task.path) || task.repository.url != repoUri))
            throw new GradleException("Refusing publication outside stage: ${task.path}")
    }
}
)BLATMAN";

std::string fileUri(const fs::path& path)
{
    const char* digits = "0123456789ABCDEF";
    std::string result = "file://";
    for (unsigned char ch : path.generic_string()) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '/' || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            result += static_cast<char>(ch);
        else { result += '%'; result += digits[ch >> 4]; result += digits[ch & 15]; }
    }
    return result;
}
} // namespace

std::optional<std::string> javaBuildCommand(const fs::path& project, BuildSystem system)
{
    if (system != BuildSystem::Gradle && system != BuildSystem::Maven) return std::nullopt;
    try {
        const auto java = selectJava(project, system);
        if (!java) return std::nullopt;
        return prefix(*java) + executable(project, system) +
               (system == BuildSystem::Gradle ? gradleOptions(*java) + " build" : " --batch-mode package");
    } catch (const std::exception& error) {
        std::cerr << "Cannot prepare Java build: " << error.what() << '\n';
        return std::nullopt;
    }
}

bool prepareJava(const fs::path& project, BuildSystem system, const fs::path& stage)
{
    if (system != BuildSystem::Gradle && system != BuildSystem::Maven) return false;
    try {
        if (!stage.is_absolute() || !fs::is_directory(stage) || fs::is_symlink(stage) || !fs::is_empty(stage)) {
            std::cerr << "Java stage must be an existing, empty, absolute directory.\n";
            return false;
        }
        const auto java = selectJava(project, system);
        if (!java) return false;
        TemporaryDirectory scratch;
        std::string command = "cd " + quoteShell(fs::absolute(project).string()) + " && " + prefix(*java) + executable(project, system);
        if (system == BuildSystem::Gradle) {
            const auto script = scratch.path() / "stage.init.gradle";
            std::ofstream output(script);
            output << gradleScript;
            output.close();
            if (!output) return false;
            command += gradleOptions(*java) + " --init-script " + quoteShell(script.string()) +
                       " " + quoteShell("-Dblatman.stage=" + fs::canonical(stage).string()) + " blatmanStageInstall";
        } else {
            const auto uri = "blatman-stage::" + fileUri(fs::canonical(stage) / "usr/share/maven-repo");
            command += " --batch-mode " + quoteShell("-Dmaven.repo.local=" + (scratch.path() / "dependency-cache").string()) +
                       " " + quoteShell("-DaltDeploymentRepository=" + uri) +
                       " " + quoteShell("-DaltReleaseDeploymentRepository=" + uri) +
                       " " + quoteShell("-DaltSnapshotDeploymentRepository=" + uri) +
                       " -DdeployAtEnd=true package org.apache.maven.plugins:maven-deploy-plugin:3.1.4:deploy";
        }
        const auto result = runShell(command);
        if (!result.success) return false;
        if (fs::is_empty(stage)) { std::cerr << "Java staging produced no installable artifacts.\n"; return false; }
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Cannot stage Java project: " << error.what() << '\n';
        return false;
    }
}
