#include "operations/installation/details/installationHelpers.hpp"

#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;

namespace installation_detail
{

bool fail(const std::string& message)
{
    std::cerr
        << "Preparacao da instalacao: "
        << message << '\n';

    return false;
}

bool validStage(const fs::path& stage)
{
    if (geteuid() == 0)
        return fail("execute a preparação como usuário comum.");

    if (!stage.is_absolute() ||
        stage != stage.lexically_normal() ||
        fs::symlink_status(stage).type() != fs::file_type::directory ||
        fs::canonical(stage) != stage ||
        !fs::is_empty(stage))
    {
        return fail(
            "o staging deve ser um diretório absoluto, real e vazio."
        );
    }

    return true;
}

fs::path directory(const fs::path& path)
{
    const fs::path result = fs::canonical(path);

    if (!fs::is_directory(result))
    {
        throw std::runtime_error(
            "diretório de projeto invalido: " +
            path.string()
        );
    }

    return result;
}

bool within(const fs::path& path, const fs::path& root)
{
    auto p = path.begin();

    for (auto r = root.begin(); r != root.end(); ++r, ++p)
    {
        if (p == path.end() || *p != *r)
            return false;
    }

    return true;
}

bool populated(const fs::path& stage)
{
    if (fs::symlink_status(stage).type() != fs::file_type::directory ||
        fs::canonical(stage) != stage ||
        fs::is_empty(stage))
    {
        return fail(
            "o instalador não produziu um staging valido."
        );
    }

    return true;
}

}