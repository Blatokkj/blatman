
#include "operations/builder/mozillaSupport.hpp"
#include "shell.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{

BuildResult failMozilla(
    const std::string& message,
    const std::string& output = {}
)
{
    std::cerr << message << '\n';

    return {
        false,
        output + message + "\n",
        {}
    };
}

CommandResult runMach(
    const fs::path& project,
    const std::string& arguments,
    bool echo
)
{
    return runShell(
        "cd " + quoteShell(project.string()) +
        " && ./mach " + arguments,
        echo
    );
}

}

BuildResult mozillaBuild(const fs::path& project)
{
    std::string output;

    try
    {
        const fs::path source = fs::canonical(project);

        if (!fs::is_regular_file(source / "mach"))
            return failMozilla("Executavel mach nao encontrado.");

        std::cout << "Mozilla Mach: compilando projeto...\n";

        const auto built = runMach(source, "build", true);

        output += built.output;

        if (!built.success)
        {
            return failMozilla(
                "Falha na compilacao Mozilla Mach.",
                output
            );
        }

        return {true, output, {}};
    }
    catch (const std::exception& error)
    {
        return failMozilla(
            std::string("Falha no suporte Mozilla Mach: ") +
            error.what(),
            output
        );
    }
}
