#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include <iostream>

namespace
{
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* red = "\033[38;2;255;70;70m";
    constexpr const char* gray = "\033[38;2;150;150;150m";
    constexpr const char* white = "\033[38;2;240;240;240m";
}

namespace nexo::commands
{
    int help()
    {
        std::cout
            << '\n'
            << red << bold
            << "  NEXO"
            << reset
            << gray << "  // LINUX TOOLS"
            << reset << '\n'
            << gray
            << "  A versatile command-line toolkit for Linux."
            << reset << "\n\n"

            << white << bold << "  USAGE" << reset << '\n'
            << "    nexo " << gray << "<command> [options]" << reset
            << "\n\n"

            << white << bold << "  COMMANDS" << reset << '\n'
            << "    " << red << "help" << reset
            << "       Show this help message\n"
            << "    " << red << "version" << reset
            << "    Show Nexo version information\n"
            << "    " << red << "sysinfo" << reset
            << "    Show system information\n"
            << "    " << red << "download" << reset
            << "   Download a file from a URL\n"
            << "    " << red << "copy" << reset
            << "       Copy and paste files\n"
            << "    " << red << "move" << reset
            << "       Move files\n"
            << '\n';

        return 0;
    }
}
