#include "operations/builder/mesonSupport.hpp"
#include "shell.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
using Json = nlohmann::json;

struct InstallEntry
{
    std::string source;
    fs::path destination;
};

struct FeatureOption
{
    std::string name;
    std::string value;
};

BuildResult failure(const std::string& message,
                    const std::string& output = {})
{
    std::cerr << message << '\n';
    return {false, output + message + '\n', {}};
}

std::string sourceCommand(const fs::path& source,
                          const std::string& command)
{
    return "cd " + quoteShell(source.string()) + " && " + command;
}

Json introspect(const fs::path& source,
                const std::string& option)
{
    const auto result = runShell(
        sourceCommand(
            source,
            "meson introspect build " + option
        ),
        false
    );

    if (!result.success)
        throw std::runtime_error(
            "Falha na introspecção Meson:\n" + result.output
        );

    Json data = Json::parse(result.output, nullptr, false);

    if (data.is_discarded())
        throw std::runtime_error(
            "O Meson retornou JSON inválido."
        );

    return data;
}

bool withinPrefix(const fs::path& destination,
                  const fs::path& prefix)
{
    if (!destination.is_absolute() || !prefix.is_absolute())
        return false;

    const auto target = destination.lexically_normal();
    const auto root = prefix.lexically_normal();

    auto targetIt = target.begin();

    for (auto rootIt = root.begin(); rootIt != root.end();
         ++rootIt, ++targetIt)
    {
        if (targetIt == target.end() || *targetIt != *rootIt)
            return false;
    }

    return true;
}

std::vector<InstallEntry> externalInstallEntries(
    const fs::path& source,
    const fs::path& prefix)
{
    const Json installed = introspect(source, "--installed");

    if (!installed.is_object())
        throw std::runtime_error(
            "O plano de instalação Meson não é um objeto JSON."
        );

    std::vector<InstallEntry> external;

    for (auto it = installed.begin(); it != installed.end(); ++it)
    {
        if (!it.value().is_string())
            throw std::runtime_error(
                "Destino inválido no plano de instalação Meson."
            );

        const fs::path destination = it.value().get<std::string>();

        if (!destination.is_absolute())
            throw std::runtime_error(
                "O Meson retornou um destino não absoluto: " +
                destination.string()
            );

        if (!withinPrefix(destination, prefix))
        {
            external.push_back({
                it.key(),
                destination
            });
        }
    }

    return external;
}

bool hasChoice(const Json& option, const std::string& choice)
{
    const auto it = option.find("choices");

    if (it == option.end() || !it->is_array())
        return false;

    return std::any_of(
        it->begin(),
        it->end(),
        [&](const Json& value)
        {
            return value.is_string() &&
                   value.get<std::string>() == choice;
        }
    );
}

std::vector<FeatureOption> automaticFeatures(
    const fs::path& source)
{
    const Json options = introspect(source, "--buildoptions");

    if (!options.is_array())
        throw std::runtime_error(
            "Lista de opções Meson inválida."
        );

    std::vector<FeatureOption> features;

    for (const auto& option : options)
    {
        if (!option.is_object())
            continue;

        // Apenas opções declaradas pelo projeto e seus subprojetos.
        if (option.value("section", std::string{}) != "user")
            continue;

        // UserFeatureOption aparece como combo na introspecção.
        if (option.value("type", std::string{}) != "combo")
            continue;

        const std::string value =
            option.value("value", std::string{});

        if (value != "auto")
            continue;

        if (!hasChoice(option, "auto") ||
            !hasChoice(option, "enabled") ||
            !hasChoice(option, "disabled"))
            continue;

        const std::string name =
            option.value("name", std::string{});

        if (name.empty())
            continue;

        features.push_back({name, value});
    }

    return features;
}

CommandResult setFeature(const fs::path& source,
                         const FeatureOption& feature,
                         const std::string& value)
{
    const std::string argument =
        "-D" + feature.name + "=" + value;

    return runShell(
        sourceCommand(
            source,
            "meson configure build " + quoteShell(argument)
        ),
        true
    );
}

bool restoreFeature(const fs::path& source,
                    const FeatureOption& feature)
{
    const auto restored = setFeature(
        source,
        feature,
        feature.value
    );

    return restored.success;
}

bool adaptExternalFeatures(
    const fs::path& source,
    const fs::path& prefix,
    std::vector<InstallEntry>& external,
    std::string& output)
{
    const auto features = automaticFeatures(source);

    for (const auto& feature : features)
    {
        if (external.empty())
            break;

        std::cout
            << "Blatman: verificando feature opcional '"
            << feature.name << "'...\n";

        const auto disabled = setFeature(
            source,
            feature,
            "disabled"
        );

        output += disabled.output;

        if (!disabled.success)
        {
            if (!restoreFeature(source, feature))
                throw std::runtime_error(
                    "Não foi possível restaurar a feature '" +
                    feature.name + "'."
                );

            continue;
        }

        std::vector<InstallEntry> updated;

        try
        {
            updated = externalInstallEntries(source, prefix);
        }
        catch (...)
        {
            if (!restoreFeature(source, feature))
                throw std::runtime_error(
                    "Falha ao restaurar a feature após erro de introspecção."
                );

            throw;
        }

        // Só mantém a alteração se ela reduzir os destinos externos.
        if (updated.size() < external.size())
        {
            std::cout
                << "Blatman: feature '" << feature.name
                << "' desativada. Destinos externos reduzidos de "
                << external.size() << " para "
                << updated.size() << ".\n";

            external = std::move(updated);
            continue;
        }

        // A feature não resolveu o problema: restaura seu estado original.
        if (!restoreFeature(source, feature))
            throw std::runtime_error(
                "Não foi possível restaurar a feature '" +
                feature.name + "'."
            );

        external = externalInstallEntries(source, prefix);
    }

    return external.empty();
}

std::string describeExternalEntries(
    const std::vector<InstallEntry>& external,
    const fs::path& prefix)
{
    std::ostringstream message;

    message
        << "O plano de instalação Meson contém destinos fora de "
        << prefix.string() << ":\n";

    for (const auto& entry : external)
    {
        message
            << "  " << entry.destination
            << " (origem: " << entry.source << ")\n";
    }

    message
        << "Nenhuma feature opcional identificada conseguiu "
        << "corrigir esses destinos.";

    return message.str();
}
}

BuildResult buildMesonProject(
    const fs::path& source,
    const fs::path& prefix)
{
    std::string output;

    try
    {
        if (!prefix.is_absolute())
            return failure(
                "O prefixo Meson precisa ser absoluto."
            );

        const bool reconfigure = fs::is_regular_file(
            source / "build" /
            "meson-private" / "coredata.dat"
        );

        std::string setup =
            "meson setup " +
            std::string(reconfigure ? "--reconfigure " : "") +
            "build " +
            quoteShell("--prefix=" + prefix.string()) +
            " --bindir=bin --libdir=lib";

        const auto configured = runShell(
            sourceCommand(source, setup),
            true
        );

        output += configured.output;

        if (!configured.success)
            return failure(
                "Falha ao configurar o projeto Meson.",
                output
            );

        // Inspeciona os destinos antes de compilar.
        auto external = externalInstallEntries(source, prefix);

        if (!external.empty())
        {
            std::cout
                << "Blatman: foram encontrados "
                << external.size()
                << " destinos fora do prefixo.\n";

            if (!adaptExternalFeatures(
                    source, prefix, external, output))
            {
                return failure(
                    describeExternalEntries(external, prefix),
                    output
                );
            }
        }

        // Só compila depois de validar o plano de instalação.
        const auto compiled = runShell(
            sourceCommand(
                source,
                "meson compile -C build"
            ),
            true
        );

        output += compiled.output;

        if (!compiled.success)
            return failure(
                "Falha ao compilar o projeto Meson.",
                output
            );

        return {true, output, {}};
    }
    catch (const std::exception& error)
    {
        return failure(
            std::string("Falha no suporte Meson: ") + error.what(),
            output
        );
    }
}
