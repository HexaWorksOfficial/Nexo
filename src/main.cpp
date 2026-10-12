#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include <string_view>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return nexo::commands::help();
    }

    const std::string_view command = argv[1];

    if (command == "help" || command == "--help" || command == "-h")
    {
        return nexo::commands::help();
    }

    if (command == "version" || command == "--version" || command == "-v")
    {
        return nexo::commands::version();
    }

    if (command == "sysinfo")
    {
        return nexo::commands::sysinfo(argc - 1, argv + 1);
    }

    if (command == "download")
    {
        return nexo::commands::download(argc - 1, argv + 1);
    }

    if (command == "copy")
    {
        return nexo::commands::copy(argc - 1, argv + 1);
    }

    if (command == "move")
    {
        return nexo::commands::move(argc - 1, argv + 1);
    }

    if (command == "meow")
    {
        return nexo::commands::meow(argc - 1, argv + 1);
    }

    nexo::print_unknown_command(command);
    return 2;
}
