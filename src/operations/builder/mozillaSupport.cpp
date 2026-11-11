#include "operations/builder/mozillaSupport.hpp"
#include "shell.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace
{
using Json = nlohmann::json;

BuildResult failMozilla(const std::string& message,
                        const std::string& output = {})
{
    std::cerr << message << '\n';
    return {false, output + message + "\n", {}, {}};
}

CommandResult runMach(const fs::path& project,
                      const std::string& arguments,
                      bool echo)
{
    return runShell(
        "cd " + quoteShell(project.string()) +
        " && ./mach " + arguments,
        echo
    );
}

// Extracts the first valid JSON object, ignoring surrounding diagnostics.
std::optional<Json> extractJsonObject(const std::string& output)
{
    for (std::size_t start = output.find('{');
         start != std::string::npos;
         start = output.find('{', start + 1))
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
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    quoted = false;
            }
            else if (c == '"')
                quoted = true;
            else if (c == '{')
                ++depth;
            else if (c == '}' && --depth == 0)
                break;
        }

        if (end == output.size())
            continue;

        Json object = Json::parse(
            output.substr(start, end - start + 1),
            nullptr,
            false
        );

        if (!object.is_discarded() && object.is_object())
            return object;
    }

    return std::nullopt;
}

}

BuildResult mozillaBuild(const fs::path& project)
{
    try
    {
        const fs::path source = fs::canonical(project);

        if (!fs::is_regular_file(source / "mach"))
            return failMozilla("Executavel mach nao encontrado.");

        BuildResult result{true, {}, {}, {}};

        // 1. Compile the project.
        std::cout << "Mozilla Mach: compilando projeto...\n";

        const auto built = runMach(source, "build", true);

        result.output += built.output;

        if (!built.success)
            return failMozilla("Falha na compilacao Mozilla Mach.",
                               result.output);

        // 2. Check whether this project provides the package command.
        const auto packageHelp = runMach(source, "help package", false);

        if (!packageHelp.success)
        {
            std::cerr
                << "Mozilla Mach: comando 'package' indisponivel. "
                << "Build concluido, sem distribuicao instalavel.\n";

            return result;
        }

        // 3. Generate the distribution.
        std::cout << "Mozilla Mach: gerando distribuicao...\n";

        const auto packaged = runMach(source, "package", true);

        result.output += packaged.output;

        if (!packaged.success)
            return failMozilla("Falha ao gerar a distribuicao Mozilla Mach.",
                               result.output);

        // 4. Query the actual object directory.
        const auto environment = runMach(
            source,
            "environment --format json",
            false
        );

        if (!environment.success)
            return failMozilla(
                "Falha ao consultar o ambiente Mozilla Mach.",
                result.output
            );

        const auto metadata = extractJsonObject(environment.output);

        if (!metadata ||
            !metadata->contains("topobjdir") ||
            !metadata->at("topobjdir").is_string())
        {
            return failMozilla(
                "Metadados Mozilla Mach invalidos: topobjdir ausente.",
                result.output
            );
        }

        fs::path objectDirectory =
            metadata->at("topobjdir").get<std::string>();

        if (objectDirectory.empty())
            return failMozilla("Diretorio de objetos Mozilla Mach vazio.",
                               result.output);

        if (objectDirectory.is_relative())
            objectDirectory = source / objectDirectory;

        objectDirectory = fs::weakly_canonical(objectDirectory);

        if (!fs::is_directory(objectDirectory))
            return failMozilla(
                "Diretorio de objetos Mozilla Mach nao encontrado: " +
                objectDirectory.string(),
                result.output
            );

        // 5. Resolve the generated distribution.
        const fs::path distribution = objectDirectory / "dist";

        if (!fs::is_directory(distribution) || fs::is_empty(distribution))
            return failMozilla(
                "Distribuicao Mozilla Mach inexistente ou vazia: " +
                distribution.string(),
                result.output
            );

        result.distribution = fs::canonical(distribution);

        return result;
    }
    catch (const std::exception& error)
    {
        return failMozilla(
            std::string("Falha no suporte Mozilla Mach: ") + error.what()
        );
    }
}