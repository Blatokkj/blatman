#pragma once

#include <filesystem>
#include <vector>

// Preparação de instalações sem publicação.
// O chamador fornece um diretório staging absoluto, existente e vazio.

bool prepareCMake(
    const std::filesystem::path& buildDir,
    const std::filesystem::path& stage
);

bool prepareMeson(
    const std::filesystem::path& buildDir,
    const std::filesystem::path& stage
);

bool prepareMake(
    const std::filesystem::path& source,
    const std::filesystem::path& stage
);

bool prepareNpm(
    const std::filesystem::path& source,
    const std::filesystem::path& stage
);

bool prepareMozillaMach(
    const std::filesystem::path& source,
    const std::filesystem::path& stage
);

bool prepareExecutables(
    const std::vector<std::filesystem::path>& paths,
    const std::filesystem::path& stage
);