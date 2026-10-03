#include "operations/install.hpp"

#include <cstdlib>

bool Install::clone(
    const std::string& url,
    const fs::path& destination) const
{
    if (!fs::exists(destination))
    {
        std::string command =
            "git clone \"" +
            url +
            "\" \"" +
            destination.string() +
            "\"";

        return std::system(command.c_str()) == 0;
    }
    else
    {
        return true;
    }
}