#include "Installer.hpp"
#include "Logger.hpp"
#include "GitRepository.hpp"
#include "BuildDetector.hpp"
#include "Builder.hpp"
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <iostream>
#include <array>
#include <memory>

Installer::Installer(const fs::path& cacheDir, const fs::path& binDir, const fs::path& manifestDir)
    : cacheDir_(cacheDir), binDir_(binDir), manifestDir_(manifestDir) {
    if (!fs::exists(binDir_)) {
        fs::create_directories(binDir_);
    }
    if (!fs::exists(manifestDir_)) {
        fs::create_directories(manifestDir_);
    }
}

bool Installer::install(const fs::path& repoPath, const std::string& buildSystem, const std::string& repoUrl) {
    Logger logger(fs::path(getenv("HOME")) / ".blatman" / "logs");
    logger.logInfo("Iniciando instalacao do pacote em: " + repoPath.string() + " (build system: " + buildSystem + ")");

    std::vector<fs::path> binaries = detectBinaries(repoPath, buildSystem);
    
    // For Maven with wrapper strategy, generate wrapper script if no native binary found
    if (buildSystem == "Maven" && binaries.empty()) {
        logger.logInfo("Nenhum binario nativo encontrado, tentando gerar wrapper script para Maven...");
        auto wrapperPath = generateMavenWrapperScript(repoPath, buildSystem);
        if (wrapperPath) {
            // Wrapper is already in binDir_, don't add to binaries for copying
            logger.logInfo("Wrapper script gerado e instalado: " + wrapperPath->string());
            addToPath();
            logger.logInfo("Instalacao concluida com sucesso via wrapper script.");
            return true;
        }
    }
    
    if (binaries.empty()) {
        logger.logError("Nenhum binario detectado em: " + repoPath.string());
        return false;
    }

    if (!copyBinaries(binaries)) {
        logger.logError("Falha ao copiar binarios para: " + binDir_.string());
        return false;
    }

    if (!generateManifest(repoPath, binaries, buildSystem)) {
        logger.logError("Falha ao gerar manifesto para: " + repoPath.string());
        return false;
    }

    logger.logInfo("Instalacao concluida com sucesso. Binarios instalados: " + std::to_string(binaries.size()));
    return true;
}

std::vector<fs::path> Installer::detectBinaries(const fs::path& repoPath, const std::string& buildSystem) const {
    std::vector<fs::path> binaries;

    if (buildSystem == "Cargo" || buildSystem == "1") {
        fs::path targetDir = repoPath / "target" / "release";
        if (fs::exists(targetDir)) {
            for (const auto& entry : fs::directory_iterator(targetDir)) {
                if (entry.is_regular_file() && (entry.path().extension().empty() || entry.path().extension() == ".bin")) {
                    struct stat st;
                    if (stat(entry.path().c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                        binaries.push_back(entry.path());
                    }
                }
            }
        }
    } else if (buildSystem == "CMake") {
        std::vector<fs::path> cmakeDirs = {
            repoPath / "build",
            repoPath / "bin"
        };
        for (const auto& dir : cmakeDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Meson") {
        std::vector<fs::path> mesonDirs = {
            repoPath / "build",
            repoPath / "out"
        };
        for (const auto& dir : mesonDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Npm") {
        std::vector<fs::path> npmDirs = {
            repoPath / "dist",
            repoPath / "build",
            repoPath / "bin"
        };
        for (const auto& dir : npmDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Gradle") {
        std::vector<fs::path> gradleDirs = {
            repoPath / "build" / "install" / repoPath.filename() / "bin",
            repoPath / "build" / "libs",
            repoPath / "app" / "build" / "install" / "app" / "bin"
        };
        for (const auto& dir : gradleDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Autotools") {
        std::vector<fs::path> autotoolsDirs = {
            repoPath / "src",
            repoPath / "bin",
            repoPath
        };
        for (const auto& dir : autotoolsDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Maven") {
        // Maven: detectar wrapper script, native binary, ou JAR
        std::vector<fs::path> mavenDirs = {
            repoPath / "target"
        };
        
        for (const auto& dir : mavenDirs) {
            if (fs::exists(dir)) {
                // Procura por native binary (GraalVM)
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        struct stat st;
                        if (stat(entry.path().c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                            std::string name = entry.path().filename().string();
                            std::string ext = entry.path().extension().string();
                            
                            // Native binary (sem extensão, executável)
                            // JAR com Main-Class
                            if (ext == ".jar" || ext.empty()) {
                                binaries.push_back(entry.path());
                            }
                        }
                    }
                }
            }
        }
        
        // Se não encontrou binários, tenta achar wrapper script ou JAR principal
        if (binaries.empty()) {
            for (const auto& entry : fs::recursive_directory_iterator(repoPath)) {
                if (entry.is_regular_file()) {
                    struct stat st;
                    if (stat(entry.path().c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                        std::string name = entry.path().filename().string();
                        if (name.find("mvn") == 0 || name == "wrapper" || name.find("run") == 0) {
                            binaries.push_back(entry.path());
                        }
                    }
                }
            }
        }
    } else if (buildSystem == "Go") {
        // Go binaries are typically in the repo root or cmd/ subdirectories
        std::vector<fs::path> goDirs = {
            repoPath,
            repoPath / "cmd",
            repoPath / "build"
        };
        for (const auto& dir : goDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else if (buildSystem == "Make") {
        std::vector<fs::path> makeDirs = {
            repoPath / "build",
            repoPath / "bin",
            repoPath
        };
        for (const auto& dir : makeDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    } else {
        std::vector<fs::path> commonDirs = {
            repoPath / "build",
            repoPath / "bin",
            repoPath / "out",
            repoPath / "dist"
        };

        for (const auto& dir : commonDirs) {
            if (fs::exists(dir)) {
                auto found = findExecutablesInDir(dir);
                binaries.insert(binaries.end(), found.begin(), found.end());
            }
        }
    }

    std::sort(binaries.begin(), binaries.end());
    binaries.erase(std::unique(binaries.begin(), binaries.end()), binaries.end());

    return binaries;
}

std::vector<fs::path> Installer::findExecutablesInDir(const fs::path& dir) const {
    std::vector<fs::path> binaries;
    if (!fs::exists(dir)) return binaries;

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            struct stat st;
            if (stat(entry.path().c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                std::string name = entry.path().filename().string();
                std::string ext = entry.path().extension().string();
                
                if (name != "CMakeCache.txt" && name != "Makefile" && 
                    ext != ".o" && ext != ".a" &&
                    ext != ".so" && ext != ".dylib" &&
                    ext != ".dll" && ext != ".lib" &&
                    ext != ".sh" && ext != ".py" && ext != ".pl" &&
                    ext != ".sample" && ext != ".txt" && ext != ".md" &&
                    ext != ".cmake" && ext != ".in" && ext != ".am" &&
                    ext != ".ac" && ext != ".m4" && ext != ".pc" &&
                    name != "configure" && name != "config.guess" && name != "config.sub" &&
                    name != "install-sh" && name != "missing" && name != "depcomp" &&
                    name != "test" && name != "tests" && name != "run-tests" &&
                    name != "libtool" && name != "ltmain.sh" &&
                    name.find("test") != 0 && name.find("Test") != 0 &&
                    name.find("sample") == std::string::npos &&
                    name.find("example") == std::string::npos &&
                    name.find("CMakeDetermineCompilerABI") == std::string::npos &&
                    name.find("applypatch") == std::string::npos &&
                    name.find("commit-msg") == std::string::npos &&
                    name.find("fsmonitor") == std::string::npos &&
                    name.find("post-update") == std::string::npos &&
                    name.find("pre-applypatch") == std::string::npos &&
                    name.find("pre-commit") == std::string::npos &&
                    name.find("pre-merge-commit") == std::string::npos &&
                    name.find("pre-push") == std::string::npos &&
                    name.find("pre-rebase") == std::string::npos &&
                    name.find("pre-receive") == std::string::npos &&
                    name.find("prepare-commit-msg") == std::string::npos &&
                    name.find("push-to-checkout") == std::string::npos &&
                    name.find("sendemail-validate") == std::string::npos &&
                    name.find("update.sample") == std::string::npos) {
                    binaries.push_back(entry.path());
                }
            }
        }
    }
    return binaries;
}

bool Installer::copyBinaries(const std::vector<fs::path>& binaries) {
    for (const auto& binary : binaries) {
        fs::path dest = binDir_ / binary.filename();
        try {
            fs::copy_file(binary, dest, fs::copy_options::overwrite_existing);
            fs::permissions(dest, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                           fs::perm_options::add);
        } catch (const fs::filesystem_error& e) {
            return false;
        }
    }
    return true;
}

bool Installer::generateManifest(const fs::path& repoPath, const std::vector<fs::path>& binaries, const std::string& buildSystem, const std::string& repoUrl) {
    PackageManifest manifest;
    manifest.buildSystem = buildSystem;

    if (fs::exists(repoPath / "Cargo.toml")) {
        auto cargoManifest = parseCargoToml(repoPath);
        if (cargoManifest) {
            manifest = *cargoManifest;
            manifest.buildSystem = buildSystem;
        } else {
            manifest.name = repoPath.filename().string();
            manifest.buildCommand = "cargo build --release --bins";
            for (const auto& binary : binaries) {
                manifest.binaries.push_back(binary.filename().string());
            }
        }
    } else {
        manifest.name = repoPath.filename().string();
        if (buildSystem == "CMake") manifest.buildCommand = "cmake . && make";
        else if (buildSystem == "Make") manifest.buildCommand = "make";
        else if (buildSystem == "Meson") manifest.buildCommand = "meson setup build && ninja -C build";
        else if (buildSystem == "Npm") manifest.buildCommand = "npm install && npm run build";
        else if (buildSystem == "Maven") manifest.buildCommand = "mvn package";
        else if (buildSystem == "Gradle") manifest.buildCommand = "gradle build";
        else if (buildSystem == "Autotools") manifest.buildCommand = "./configure && make";
        else manifest.buildCommand = "unknown";
        
        for (const auto& binary : binaries) {
            manifest.binaries.push_back(binary.filename().string());
        }
    }

    fs::path manifestPath = manifestDir_ / (manifest.name + ".json");
    std::ofstream manifestFile(manifestPath);
    if (!manifestFile.is_open()) {
        return false;
    }

    manifestFile << "{\n";
    manifestFile << "    \"name\": \"" << manifest.name << "\",\n";
    manifestFile << "    \"buildSystem\": \"" << manifest.buildSystem << "\",\n";
    manifestFile << "    \"buildCommand\": \"" << manifest.buildCommand << "\",\n";
    manifestFile << "    \"binaries\": [";
    for (size_t i = 0; i < manifest.binaries.size(); ++i) {
        manifestFile << "\"" << manifest.binaries[i] << "\"";
        if (i != manifest.binaries.size() - 1) manifestFile << ", ";
    }
    manifestFile << "],\n";
    manifestFile << "    \"dependencies\": [";
    for (size_t i = 0; i < manifest.dependencies.size(); ++i) {
        manifestFile << "\"" << manifest.dependencies[i] << "\"";
        if (i != manifest.dependencies.size() - 1) manifestFile << ", ";
    }
    manifestFile << "]\n";
    manifestFile << "}\n";
    manifestFile.close();
    return true;
}

std::optional<PackageManifest> Installer::parseCargoToml(const fs::path& repoPath) const {
    PackageManifest manifest;
    fs::path cargoToml = repoPath / "Cargo.toml";
    std::ifstream file(cargoToml);
    if (!file.is_open()) return std::nullopt;

    std::string line;
    bool inBinSection = false;

    while (std::getline(file, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        if (trimmed.rfind("name", 0) == 0 && trimmed.find("=") != std::string::npos) {
            size_t start = trimmed.find('"');
            size_t end = trimmed.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                manifest.name = trimmed.substr(start + 1, end - start - 1);
            }
        }
        if (trimmed == "[[bin]]") {
            inBinSection = true;
        } else if (trimmed.rfind("[", 0) == 0 && trimmed != "[[bin]]") {
            inBinSection = false;
        }
        if (inBinSection && trimmed.rfind("name", 0) == 0 && trimmed.find("=") != std::string::npos) {
            size_t start = trimmed.find('"');
            size_t end = trimmed.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                manifest.binaries.push_back(trimmed.substr(start + 1, end - start - 1));
            }
        }
    }
    manifest.buildCommand = "cargo build --release --bins";
    return manifest;
}

bool Installer::upgradePackage(const std::string& packageName) {
    Logger logger(fs::path(getenv("HOME")) / ".blatman" / "logs");
    
    fs::path manifestPath = manifestDir_ / (packageName + ".json");
    if (!fs::exists(manifestPath)) {
        logger.logError("Pacote nao encontrado: " + packageName);
        std::cerr << "Erro: Pacote '" << packageName << "' nao esta instalado." << std::endl;
        return false;
    }
    
    // Parse manifest to get repo info
    std::ifstream manifestFile(manifestPath);
    if (!manifestFile.is_open()) {
        logger.logError("Falha ao ler manifesto: " + manifestPath.string());
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(manifestFile)), std::istreambuf_iterator<char>());
    manifestFile.close();
    
    // Simple JSON parsing for repo URL and build info
    // The manifest doesn't store the original URL, so we need to store it
    // For now, we'll just rebuild from the cached source
    fs::path cacheDir = cacheDir_ / packageName;
    if (!fs::exists(cacheDir)) {
        logger.logError("Cache do pacote nao encontrado: " + cacheDir.string());
        std::cerr << "Erro: Cache do pacote nao encontrado. Reinstale com 'blatman install <URL>'" << std::endl;
        return false;
    }
    
    logger.logInfo("Atualizando pacote: " + packageName);
    
    // Update the repo
    GitRepository git;
    if (!git.pull(cacheDir)) {
        logger.logError("Falha ao atualizar repositorio: " + packageName);
        std::cerr << "Erro: Falha ao atualizar repositorio." << std::endl;
        return false;
    }
    
    // Detect build system again
    BuildDetector detector;
    BuildSystem buildSystem = detector.detect(fs::path(cacheDir_ / packageName));
    if (buildSystem == BuildSystem::Unknown) {
        logger.logError("Sistema de build desconhecido apos atualizacao");
        return false;
    }
    
    // Rebuild
    Builder builder;
    logger.logInfo("Recompilando pacote atualizado: " + packageName);
    if (!builder.build(buildSystem, fs::path(cacheDir_ / packageName))) {
        logger.logError("Falha na recompilacao do pacote: " + packageName);
        return false;
    }
    
    logger.logInfo("Pacote atualizado com sucesso: " + packageName);
    std::cout << "Sucesso: Pacote '" << packageName << "' atualizado." << std::endl;
    return true;
}

bool Installer::upgradeAllPackages() {
    Logger logger(fs::path(getenv("HOME")) / ".blatman" / "logs");
    
    std::vector<std::string> packages;
    for (const auto& entry : fs::directory_iterator(manifestDir_)) {
        if (entry.path().extension() == ".json") {
            std::string name = entry.path().stem().string();
            packages.push_back(name);
        }
    }
    
    if (packages.empty()) {
        logger.logInfo("Nenhum pacote instalado para atualizar");
        std::cout << "Nenhum pacote instalado." << std::endl;
        return true;
    }
    
    logger.logInfo("Iniciando atualizacao de " + std::to_string(packages.size()) + " pacotes");
    std::cout << "Atualizando " << packages.size() << " pacotes..." << std::endl;
    
    int success = 0;
    int failed = 0;
    
    for (const auto& pkg : packages) {
        std::cout << "\n--- Atualizando: " << pkg << " ---" << std::endl;
        if (upgradePackage(pkg)) {
            success++;
        } else {
            failed++;
            std::cerr << "Falha ao atualizar: " << pkg << std::endl;
        }
    }
    
    std::cout << "\n=== Resumo ===" << std::endl;
    std::cout << "Sucesso: " << success << std::endl;
    std::cout << "Falhas: " << failed << std::endl;
    
    return failed == 0;
}

bool Installer::addToPath() const {
    std::string shellConfig;
    std::string shell = getenv("SHELL") ? getenv("SHELL") : "";
    if (shell.find("zsh") != std::string::npos) {
        shellConfig = fs::path(getenv("HOME")) / ".zshrc";
    } else if (shell.find("fish") != std::string::npos) {
        shellConfig = fs::path(getenv("HOME")) / ".config/fish/config.fish";
    } else {
        shellConfig = fs::path(getenv("HOME")) / ".bashrc";
    }

    std::string pathLine;
    if (shell.find("fish") != std::string::npos) {
        pathLine = "set -gx PATH $HOME/.blatman/bin $PATH";
    } else {
        pathLine = "export PATH=\"$HOME/.blatman/bin:$PATH\"";
    }

    bool pathExists = false;
    if (fs::exists(shellConfig)) {
        std::ifstream file(shellConfig);
        std::string line;
        while (std::getline(file, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
            if (trimmed == pathLine || (trimmed.find("$HOME/.blatman/bin") != std::string::npos && trimmed.find("PATH") != std::string::npos)) {
                pathExists = true;
                break;
            }
        }
    }

    if (!pathExists) {
        std::ofstream outFile(shellConfig, std::ios::app);
        if (!outFile.is_open()) {
            return false;
        }
        outFile << "\n" << pathLine << "\n";
        outFile.close();
    }
    return true;
}

std::optional<fs::path> Installer::generateMavenWrapperScript(const fs::path& repoPath, const std::string& buildSystem) const {
    // Find the main JAR in target/
    fs::path targetDir = repoPath / "target";
    if (!fs::exists(targetDir)) return std::nullopt;
    
    std::string mainJar;
    for (const auto& entry : fs::directory_iterator(targetDir)) {
        std::string name = entry.path().filename().string();
        if (name.find(".jar") != std::string::npos && 
            name.find("sources") == std::string::npos &&
            name.find("javadoc") == std::string::npos &&
            name.find("test") == std::string::npos) {
            mainJar = entry.path().string();
            break;
        }
    }
    
    if (mainJar.empty()) return std::nullopt;
    
    // Extract project name from pom.xml
    std::string projectName = repoPath.filename().string();
    fs::path pomXml = repoPath / "pom.xml";
    if (fs::exists(pomXml)) {
        std::ifstream file(pomXml);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // Find the project's artifactId (not parent's)
        // Look for artifactId that comes after groupId and version in the main project section
        size_t projectStart = content.find("<project");
        if (projectStart != std::string::npos) {
            // Find the project's artifactId after the project start
            size_t searchStart = content.find("<artifactId>", projectStart);
            // Skip parent's artifactId if present
            size_t parentStart = content.find("<parent>", projectStart);
            if (parentStart != std::string::npos) {
                size_t parentEnd = content.find("</parent>", parentStart);
                if (parentEnd != std::string::npos) {
                    searchStart = content.find("<artifactId>", parentEnd);
                }
            }
            
            if (searchStart != std::string::npos) {
                size_t artifactStart = searchStart + 12;
                size_t artifactEnd = content.find("</artifactId>", artifactStart);
                if (artifactEnd != std::string::npos) {
                    projectName = content.substr(artifactStart, artifactEnd - artifactStart);
                }
            }
        }
    }
    
    // Generate wrapper script
    fs::path wrapperPath = binDir_ / projectName;
    std::string wrapperContent = "#!/bin/sh\n";
    wrapperContent += "# Auto-generated by Blatman\n";
    wrapperContent += "exec java -jar \"" + mainJar + "\" \"$@\"\n";
    
    try {
        std::ofstream outFile(wrapperPath);
        if (!outFile.is_open()) return std::nullopt;
        outFile << wrapperContent;
        outFile.close();
        fs::permissions(wrapperPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
        return wrapperPath;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<PackageManifest> Installer::listInstalledPackages() const {
    std::vector<PackageManifest> packages;
    for (const auto& entry : fs::directory_iterator(manifestDir_)) {
        if (entry.path().extension() == ".json") {
            std::ifstream file(entry.path());
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                
                PackageManifest manifest;
                size_t nameStart = content.find("\"name\": \"");
                if (nameStart != std::string::npos) {
                    nameStart += 9;
                    size_t nameEnd = content.find("\"", nameStart);
                    if (nameEnd != std::string::npos) {
                        manifest.name = content.substr(nameStart, nameEnd - nameStart);
                    }
                }
                
                size_t bsStart = content.find("\"buildSystem\": \"");
                if (bsStart != std::string::npos) {
                    bsStart += 16;
                    size_t bsEnd = content.find("\"", bsStart);
                    if (bsEnd != std::string::npos) {
                        manifest.buildSystem = content.substr(bsStart, bsEnd - bsStart);
                    }
                }
                
                packages.push_back(manifest);
            }
        }
    }
    return packages;
}

size_t Installer::getPackageCount() const {
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(manifestDir_)) {
        if (entry.path().extension() == ".json") {
            count++;
        }
    }
    return count;
}

std::optional<PackageManifest> Installer::getPackageManifest(const std::string& name) const {
    fs::path manifestPath = manifestDir_ / (name + ".json");
    if (!fs::exists(manifestPath)) {
        return std::nullopt;
    }
    
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    PackageManifest manifest;
    size_t nameStart = content.find("\"name\": \"");
    if (nameStart != std::string::npos) {
        nameStart += 9;
        size_t nameEnd = content.find("\"", nameStart);
        if (nameEnd != std::string::npos) {
            manifest.name = content.substr(nameStart, nameEnd - nameStart);
        }
    }
    
    size_t bsStart = content.find("\"buildSystem\": \"");
    if (bsStart != std::string::npos) {
        bsStart += 16;
        size_t bsEnd = content.find("\"", bsStart);
        if (bsEnd != std::string::npos) {
            manifest.buildSystem = content.substr(bsStart, bsEnd - bsStart);
        }
    }
    
    size_t binStart = content.find("\"binaries\": [");
    if (binStart != std::string::npos) {
        binStart += 13;
        size_t binEnd = content.find("]", binStart);
        if (binEnd != std::string::npos) {
            std::string binContent = content.substr(binStart, binEnd - binStart);
            size_t pos = 0;
            while ((pos = binContent.find("\"", pos)) != std::string::npos) {
                pos++;
                size_t end = binContent.find("\"", pos);
                if (end != std::string::npos) {
                    manifest.binaries.push_back(binContent.substr(pos, end - pos));
                    pos = end + 1;
                }
            }
        }
    }
    
    return manifest;
}

std::string Installer::getCurrentCommitHash(const fs::path& repoPath) const {
    fs::path gitDir = repoPath / ".git";
    if (!fs::exists(gitDir)) {
        return "";
    }
    
    std::string command = "cd " + repoPath.string() + " && git rev-parse HEAD 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}