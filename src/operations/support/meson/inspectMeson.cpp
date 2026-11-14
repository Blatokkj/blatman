
#include "shell.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace meson_detail
{

namespace
{

using Json = nlohmann::json;

Json parseInstalled(const std::string& output)
{
    Json direct = Json::parse(output, nullptr, false);

    if (!direct.is_discarded() && direct.is_object())
        return direct;

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
            {
                quoted = true;
            }
            else if (c == '{')
            {
                ++depth;
            }
            else if (c == '}')
            {
                if (depth == 0)
                    break;

                --depth;

                if (depth == 0)
                {
                    Json parsed = Json::parse(
                        output.substr(start, end - start + 1),
                        nullptr,
                        false
                    );

                    if (!parsed.is_discarded() && parsed.is_object())
                        return parsed;

                    break;
                }
            }
        }
    }

    throw std::runtime_error(
        "Meson não retornou um objeto JSON de instalação válido."
    );
}

}

std::vector<std::string> inspectMeson(
    const fs::path& build
)
{
    const auto inspection = runShell(
        "meson introspect --installed " +
        quoteShell(build.string()),
        false
    );

    if (!inspection.success)
    {
        throw std::runtime_error(
            "não foi possível consultar os destinos Meson.\n" +
            inspection.output
        );
    }

    const Json installed = parseInstalled(inspection.output);

    if (installed.empty())
    {
        throw std::runtime_error(
            "o projeto Meson não declarou arquivos instaláveis."
        );
    }

    std::vector<std::string> destinations;

    destinations.reserve(installed.size());

    for (auto it = installed.begin(); it != installed.end(); ++it)
    {
        if (!it.value().is_string())
        {
            throw std::runtime_error(
                "destino inválido no plano de instalação Meson."
            );
        }

        destinations.push_back(
            it.value().get<std::string>()
        );
    }

    return destinations;
}

}
