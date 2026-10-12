#include "nexo/error.hpp"

#include <iostream>
#include <string>

namespace
{
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* red = "\033[38;2;255;70;70m";
    constexpr const char* gray = "\033[38;2;150;150;150m";
}

namespace nexo
{
    void print_error(std::string_view message)
    {
        std::cerr
            << '\n'
            << red << bold << "  NEXO" << reset
            << gray << "  // ERROR" << reset
            << "\n\n"
            << "  " << message << "\n\n";
    }

    void print_error_with_hint(
        std::string_view message,
        std::string_view hint)
    {
        print_error(message);

        std::cerr
            << "  Run " << red << hint << reset
            << gray << " for usage information."
            << reset << "\n\n";
    }

    void print_unknown_command(std::string_view command)
    {
        const std::string message =
            "Unknown command: " + std::string(command);

        print_error_with_hint(message, "nexo help");
    }
}