#include "operations/installation/details/mozilla.hpp"

#include "shell.hpp"
#include "operations/installation/details/installationHelpers.hpp"

#include <nlohmann/json.hpp>

#include <unistd.h>

#include <stdexcept>
#include <string>

namespace mozilla_detail
{
namespace
{
    using Json = nlohmann::json;

    Json parseEnvironment(const std::string& output)
    {
        for (std::size_t start = 0;
             (start = output.find('{', start)) != std::string::npos;
             ++start)
        {
            std::size_t depth = 0;
            bool quoted = false;
            bool escaped = false;

            for (std::size_t end = start; end < output.size(); ++end)
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

                    continue;
                }

                if (c == '"')
                    quoted = true;
                else if (c == '{')
                    ++depth;
                else if (c == '}')
                {
                    if (--depth == 0)
                    {
                        Json result = Json::parse(
                            output.substr(start, end - start + 1),
                            nullptr,
                            false
                        );

                        if (!result.is_discarded() && result.is_object())
                            return result;

                        break;
                    }
                }
            }
        }

        throw std::runtime_error(
            "Mach não retornou um ambiente JSON válido."
        );
    }

    std::string requiredString(
        const Json& object,
        const std::string& key
    )
    {
        if (!object.contains(key) || !object[key].is_string())
        {
            throw std::runtime_error(
                "Campo ausente ou inválido no ambiente Mach: " + key
            );
        }

        return object[key].get<std::string>();
    }
}

Environment inspectMozilla(const fs::path& sourceDir)
{
    const auto source =
        installation_detail::directory(sourceDir);

    const auto mach = source / "mach";

    if (!fs::is_regular_file(mach) || ::access(mach.c_str(), X_OK) != 0)
    {
        throw std::runtime_error(
            "O executável mach não existe ou não possui permissão de execução."
        );
    }

    const auto inspected = runShell(
        "cd " + quoteShell(source.string()) +
        " && ./mach environment --format=json --verbose",
        false
    );

    if (!inspected.success)
    {
        throw std::runtime_error(
            "Não foi possível consultar o ambiente Mozilla Mach.\n" +
            inspected.output
        );
    }

    const Json environment = parseEnvironment(inspected.output);

    const fs::path topsrcdir(
        requiredString(environment, "topsrcdir")
    );

    const fs::path topobjdir(
        requiredString(environment, "topobjdir")
    );

    if (!topsrcdir.is_absolute() || !topobjdir.is_absolute())
    {
        throw std::runtime_error(
            "Mach retornou diretórios que não são absolutos."
        );
    }

    const auto configuredSource =
        installation_detail::directory(topsrcdir);

    const auto object =
        installation_detail::directory(topobjdir);

    if (configuredSource != source)
    {
        throw std::runtime_error(
            "O topsrcdir configurado pelo Mach não corresponde ao repositório."
        );
    }

    // O comando install do Gecko instala em dispositivos Android.
    // Não podemos tratá-lo como uma instalação local via DESTDIR.
    if (environment.contains("substs") &&
        environment["substs"].is_object())
    {
        const auto& substs = environment["substs"];

        if (substs.contains("MOZ_WIDGET_TOOLKIT") &&
            substs["MOZ_WIDGET_TOOLKIT"].is_string() &&
            substs["MOZ_WIDGET_TOOLKIT"].get<std::string>() == "android")
        {
            throw std::runtime_error(
                "O alvo Android não oferece a instalação local esperada."
            );
        }
    }

    return {
        configuredSource,
        object
    };
}
}