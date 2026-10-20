#include "operations/build.hpp"
#include "javaSupport.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
bool buildProject(const std::string&, const Build&) { return false; }
std::optional<std::string> javaBuildCommand(const fs::path&, BuildSystem) { return std::nullopt; }
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const auto system = std::string(argv[1]) == "cargo" ? BuildSystem::Cargo : BuildSystem::Go;
    const auto result = Build{}.run(argv[2], system, argv[3]);
    nlohmann::json document{{"success", result.success}, {"output", result.output}, {"executables", nlohmann::json::array()}};
    for (const auto& path : result.executables) document["executables"].push_back(path.string());
    std::cout << "RESULT " << document.dump() << '\n';
    return result.success ? 0 : 1;
}
