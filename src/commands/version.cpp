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
    int version()
    {
        std::cout
            << '\n'
            << red << bold << "  NEXO" << reset
            << gray << "  // VERSION INFORMATION" << reset
            << "\n\n"

            << "  " << gray << "Version      " << reset
            << white << "0.1.0" << reset << '\n'

            << "  " << gray << "Language     " << reset
            << white << "C++" << reset << '\n'

            << "  " << gray << "Build system " << reset
            << white << "CMake" << reset << '\n'

            << "  " << gray << "Platform     " << reset
            << white << "Linux" << reset << '\n'

            << '\n'
            << gray << "  Made by samuelwahbe"
            << reset << '\n'
            << '\n';

        return 0;
    }
}
