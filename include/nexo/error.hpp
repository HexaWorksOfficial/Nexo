#pragma once

#include <string_view>

namespace nexo
{
    void print_error(std::string_view message);
    void print_error_with_hint(
        std::string_view message,
        std::string_view hint);

    void print_unknown_command(std::string_view command);
}