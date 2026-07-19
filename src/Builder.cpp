#include "Builder.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <array>
#include <sys/sysinfo.h>

Builder::Builder()
    : resolver_(),
      installer_(fs::path(getenv("HOME")) / ".blatman" / "cache",
                fs::path(getenv("HOME")) / ".blatman" / "bin",
                fs::path(getenv("HOME")) / ".blatman" / "manifests") {}

static std::string escapeShellArg(const std::string& arg) {
    std::string escaped = "'";
    for (char c : arg) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

static std::string buildSystemToString(BuildSystem bs) {
    switch (bs) {
        case BuildSystem::CMake: return "CMake";
        case BuildSystem::Make: return "Make";
        case BuildSystem::Meson: return "Meson";
        case BuildSystem::Cargo: return "Cargo";
        case BuildSystem::Npm: return "Npm";
        case BuildSystem::Maven: return "Maven";
        case BuildSystem::Gradle: return "Gradle";
        case BuildSystem::Autotools: return "Autotools";
        case BuildSystem::Go: return "Go";
        default: return "Unknown";
    }
}

static long getAvailableMemoryMB() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (info.freeram * info.mem_unit) / (1024 * 1024);
    }
    return 4096; // fallback
}

static bool hasSwap() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.totalswap > 0;
    }
    return false;
}

static long getSwapTotalMB() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (info.totalswap * info.mem_unit) / (1024 * 1024);
    }
    return 0;
}

static std::string analyzeMavenProject(const fs::path& repoPath) {
    fs::path pomXml = repoPath / "pom.xml";
    if (!fs::exists(pomXml)) {
        return "wrapper";
    }
    
    std::ifstream file(pomXml);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Priority 1: Wrapper script (baseline) - check if Main-Class is defined
    bool hasMainClass = (content.find("Main-Class") != std::string::npos) ||
                        (content.find("mainClass") != std::string::npos) ||
                        (content.find("<mainClass>") != std::string::npos);
    
    // Priority 2: GraalVM native-image plugin
    bool hasNativePlugin = (content.find("native-maven-plugin") != std::string::npos) ||
                           (content.find("org.graalvm.buildtools") != std::string::npos);
    
    // Priority 3: Spring Boot 3+ with native support
    bool hasSpringBootPlugin = (content.find("spring-boot-maven-plugin") != std::string::npos);
    bool isSpringBoot3Plus = false;
    if (hasSpringBootPlugin) {
        // Check Spring Boot version >= 3
        size_t pos = content.find("spring-boot");
        if (pos != std::string::npos) {
            // Look for version nearby
            std::string context = content.substr(pos, 200);
            if (context.find("3.") != std::string::npos || 
                context.find("3.0") != std::string::npos ||
                context.find("3.1") != std::string::npos ||
                context.find("3.2") != std::string::npos ||
                context.find("3.3") != std::string::npos) {
                isSpringBoot3Plus = true;
            }
        }
    }
    
    bool hasQuarkusPlugin = (content.find("quarkus-maven-plugin") != std::string::npos);
    bool hasMicronautPlugin = (content.find("micronaut-maven-plugin") != std::string::npos);
    
    // Decision logic - priority order as requested:
    // 1. Wrapper script (always works if Main-Class)
    // 2. GraalVM native-image
    // 3. Spring Boot native
    // 4. Quarkus native
    // 5. Micronaut native
    
    if (hasNativePlugin || hasQuarkusPlugin || hasMicronautPlugin) {
        // Check if GraalVM is available
        if (fs::exists("/usr/lib/jvm/graalvm") || 
            fs::exists("/opt/graalvm") ||
            system("which native-image >/dev/null 2>&1") == 0 ||
            getenv("GRAALVM_HOME")) {
            return "graalvm-native";
        }
    }
    
    if (hasSpringBootPlugin && isSpringBoot3Plus) {
        if (fs::exists("/usr/lib/jvm/graalvm") || 
            system("which native-image >/dev/null 2>&1") == 0 ||
            getenv("GRAALVM_HOME")) {
            return "springboot-native";
        }
    }
    
    // Default: wrapper script (works everywhere with Java installed)
    if (hasMainClass) {
        return "wrapper";
    }
    
    return "jar-only"; // No Main-Class, just install JAR
}

bool Builder::build(BuildSystem buildSystem, const fs::path& repoPath) {
    resolver_.setLibClangPath();
    Logger logger(fs::path(getenv("HOME")) / ".blatman" / "logs");
    logger.logInfo("Builder recebeu sistema de build: " + buildSystemToString(buildSystem));

    long availMem = getAvailableMemoryMB();
    bool swapEnabled = hasSwap();
    long swapTotalMB = getSwapTotalMB();
    logger.logInfo("Memoria disponivel: " + std::to_string(availMem) + " MB, Swap: " + (swapEnabled ? "sim (" + std::to_string(swapTotalMB) + " MB)" : "nao"));

    // Conservador: baixa memória se RAM < 4GB OU swap < 2GB
    const bool lowMemory = (availMem < 4000) || (swapEnabled && swapTotalMB < 2048);
    const int globalJobs = lowMemory ? 1 : 2;
    const std::string ltoFlag = lowMemory ? "export CARGO_PROFILE_RELEASE_LTO=false && " : "";
    const std::string makeJobs = lowMemory ? " -j1" : " -j2";
    const std::string mavenOpts = lowMemory ? " -Dmaven.compiler.fork=true -Dmaven.javadoc.skip=true -Dmaven.test.skip=true" : "";
    const std::string gradleOpts = lowMemory ? " -Dorg.gradle.jvmargs=\"-Xmx1g\" --no-daemon --no-parallel" : " --no-daemon";
    const std::string nodeOpts = lowMemory ? " --max-old-space-size=1024" : "";
    
    logger.logInfo("Modo de compilacao: " + std::string(lowMemory ? "baixa memoria (-j1, flags de memoria)" : "normal (-j2)"));

    std::string command;
    std::string repoPathEscaped = escapeShellArg(repoPath.string());
    int result = 0;

    switch (buildSystem) {
        case BuildSystem::CMake:
            command = "cd " + repoPathEscaped + " && cmake -B build -DCMAKE_BUILD_TYPE=Release . && cmake --build build --config Release" + makeJobs;
            break;
        case BuildSystem::Make:
            command = "cd " + repoPathEscaped + " && make" + makeJobs;
            break;
        case BuildSystem::Meson:
            command = "cd " + repoPathEscaped + " && meson setup build --buildtype=release && ninja -C build" + makeJobs;
            break;
        case BuildSystem::Cargo: {
            bool isWorkspace = false;
            std::ifstream cargoToml(repoPath / "Cargo.toml");
            std::string line;
            while (std::getline(cargoToml, line)) {
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                if (trimmed.rfind("[workspace]", 0) == 0) {
                    isWorkspace = true;
                    break;
                }
            }
            cargoToml.close();

            if (isWorkspace) {
                command = "cd " + repoPathEscaped + " && " + ltoFlag + "cargo build --workspace --release -j " + std::to_string(globalJobs);
            } else {
                command = "cd " + repoPathEscaped + " && " + ltoFlag + "cargo build --release --bins -j " + std::to_string(globalJobs);
            }
            break;
        }
        case BuildSystem::Npm:
            command = "cd " + repoPathEscaped + " && NODE_OPTIONS=\"" + nodeOpts + "\" npm ci && npm run build";
            break;
        case BuildSystem::Maven: {
            std::string mavenStrategy = analyzeMavenProject(repoPath);
            logger.logInfo("Estrategia Maven detectada: " + mavenStrategy);
            
            if (mavenStrategy == "graalvm-native") {
                command = "cd " + repoPathEscaped + " && MAVEN_OPTS=\"-Xmx1g\" mvn package -DskipTests -Pnative" + mavenOpts;
            } else if (mavenStrategy == "springboot-native") {
                command = "cd " + repoPathEscaped + " && MAVEN_OPTS=\"-Xmx1g\" mvn package -DskipTests -Pnative" + mavenOpts;
            } else {
                command = "cd " + repoPathEscaped + " && mvn package -DskipTests" + mavenOpts;
            }
            break;
        }
        case BuildSystem::Gradle:
            command = "cd " + repoPathEscaped + " && ./gradlew build -x test" + gradleOpts;
            break;
        case BuildSystem::Autotools:
            command = "cd " + repoPathEscaped + " && ./configure && make" + makeJobs;
            break;
        case BuildSystem::Go: {
            // Find main package
            std::string mainPkg = "./...";
            // First check root for main.go (like lazygit)
            if (fs::exists(repoPath / "main.go")) {
                mainPkg = ".";
            } else {
                fs::path cmdDir = repoPath / "cmd";
                if (fs::exists(cmdDir)) {
                    for (const auto& entry : fs::directory_iterator(cmdDir)) {
                        if (entry.is_directory()) {
                            fs::path mainGo = entry.path() / "main.go";
                            if (fs::exists(mainGo)) {
                                // Use relative path from repo root
                                mainPkg = "./cmd/" + entry.path().filename().string();
                                break;
                            }
                        }
                    }
                }
            }
            if (lowMemory) {
                command = "cd " + repoPathEscaped + " && go build -trimpath -ldflags=\"-s -w\" -p 1 " + mainPkg;
            } else {
                command = "cd " + repoPathEscaped + " && go build -trimpath -ldflags=\"-s -w\" " + mainPkg;
            }
            break;
        }
        default:
            logger.logError("Sistema de build nao suportado: " + buildSystemToString(buildSystem));
            return false;
    }

    logger.logInfo("Executando compilacao: " + command);
    result = system(command.c_str());
    logger.logInfo("Resultado da compilacao: " + std::to_string(result));

    // Se falhou e é workspace Cargo, tenta build sequencial por membro
    if (result != 0 && buildSystem == BuildSystem::Cargo) {
        bool isWorkspace = false;
        std::ifstream cargoToml(repoPath / "Cargo.toml");
        std::string line;
        while (std::getline(cargoToml, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            if (trimmed.rfind("[workspace]", 0) == 0) {
                isWorkspace = true;
                break;
            }
        }
        cargoToml.close();

        if (isWorkspace) {
            logger.logInfo("Falha no build do workspace. Tentando build sequencial por membro...");
            
            // Tenta compilar cada membro do workspace individualmente
            for (const auto& entry : fs::directory_iterator(repoPath)) {
                if (entry.is_directory() && fs::exists(entry.path() / "Cargo.toml")) {
                    std::string memberPath = escapeShellArg(entry.path().string());
                    std::string memberCmd = "cd " + memberPath + " && " + ltoFlag + "cargo build --release -j 1";
                    logger.logInfo("Compilando membro do workspace: " + entry.path().filename().string());
                    int memberResult = system(memberCmd.c_str());
                    if (memberResult != 0) {
                        logger.logError("Falha ao compilar membro: " + entry.path().string());
                        // Continua tentando outros membros
                    }
                }
            }
            
            // Tenta o workspace completo novamente após membros individuais
            logger.logInfo("Tentando workspace completo novamente...");
            command = "cd " + repoPathEscaped + " && " + ltoFlag + "cargo build --workspace --release -j 1";
            result = system(command.c_str());
            logger.logInfo("Resultado da compilacao final do workspace: " + std::to_string(result));
        }
    }

    // Se falhou e é projeto Go, tenta build por pacote
    if (result != 0 && buildSystem == BuildSystem::Go) {
        logger.logInfo("Falha no build Go. Tentando build por pacote...");
        
        // Tenta compilar cada pacote individualmente
        for (const auto& entry : fs::recursive_directory_iterator(repoPath)) {
            if (entry.is_directory() && fs::exists(entry.path() / "go.mod")) {
                std::string memberPath = escapeShellArg(entry.path().string());
                std::string memberCmd = "cd " + memberPath + " && go build -trimpath -ldflags=\"-s -w\" ./...";
                logger.logInfo("Compilando pacote Go: " + entry.path().filename().string());
                int memberResult = system(memberCmd.c_str());
                if (memberResult != 0) {
                    logger.logError("Falha ao compilar pacote: " + entry.path().string());
                }
            }
        }
        
        // Tenta o build completo novamente
        logger.logInfo("Tentando build Go completo novamente...");
        command = "cd " + repoPathEscaped + " && go build -trimpath -ldflags=\"-s -w\" ./...";
        result = system(command.c_str());
        logger.logInfo("Resultado da compilacao final Go: " + std::to_string(result));
    }

    if (result != 0) {
        logger.logError("Falha na compilacao. Comando: " + command);
        std::string output = executeCommand(command);
        logger.logError("Saida do comando: " + output);
        
        if (resolver_.checkAndInstall(output)) {
            logger.logInfo("Tentando compilar novamente apos resolver dependencias.");
            result = system(command.c_str());
            logger.logInfo("Resultado da segunda compilacao: " + std::to_string(result));
            if (result != 0) {
                logger.logError("Falha na compilacao mesmo apos resolver dependencias.");
                return false;
            }
        } else {
            return false;
        }
    }

    logger.logInfo("Compilacao concluida com sucesso. Comando: " + command);

    if (!installer_.install(repoPath, buildSystemToString(buildSystem))) {
        logger.logError("Falha ao instalar binarios.");
        return false;
    }
    installer_.addToPath();

    return true;
}

std::string Builder::executeCommand(const std::string& command) {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((command + " 2>&1").c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Falha ao executar comando.");
    }
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::cout << buffer.data();
        std::cout.flush();
        result += buffer.data();
    }
    int status = pclose(pipe.release());
    return result;
}