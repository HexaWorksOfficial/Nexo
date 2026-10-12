#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* red = "\033[38;2;255;70;70m";
    constexpr const char* gray = "\033[38;2;150;150;150m";
    constexpr const char* white = "\033[38;2;240;240;240m";
    constexpr const char* green = "\033[38;2;100;230;130m";
    constexpr const char* yellow = "\033[38;2;255;220;70m";

    namespace fs = std::filesystem;

    void print_header()
    {
        std::cout
            << '\n'
            << red << bold << "  NEXO" << reset
            << gray << "  // COPY" << reset
            << "\n\n";
    }

    void print_usage()
    {
        std::cout
            << '\n'
            << white << bold << "  USAGE" << reset << '\n'
            << "    nexo copy <source> <destination>\n\n";
    }

    std::string format_size(std::uint64_t bytes)
    {
        double size = static_cast<double>(bytes);
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        std::size_t unit = 0;

        while (size >= 1024.0 && unit < 4)
        {
            size /= 1024.0;
            ++unit;
        }

        std::ostringstream output;
        output << std::fixed
               << std::setprecision(size < 10.0 ? 1 : 0)
               << size << ' ' << units[unit];

        return output.str();
    }

    void print_progress(std::uint64_t transferred, std::uint64_t total)
    {
        constexpr std::size_t bar_width = 30;

        const double ratio = total == 0
            ? 1.0
            : std::min(
                  1.0,
                  static_cast<double>(transferred) /
                      static_cast<double>(total));

        const auto filled = static_cast<std::size_t>(
            ratio * static_cast<double>(bar_width));

        std::cout << '\r' << "\033[K" << "  "
                  << white << '[' << reset;

        for (std::size_t i = 0; i < bar_width; ++i)
        {
            if (i < filled)
            {
                std::cout << green << '=';
            }
            else if (i == filled && ratio < 1.0)
            {
                std::cout << yellow << '>';
            }
            else
            {
                std::cout << reset << '-';
            }
        }

        std::cout << reset << white << ']'
                  << reset << ' '
                  << std::fixed << std::setprecision(1)
                  << ratio * 100.0 << "%  "
                  << format_size(transferred)
                  << " / " << format_size(total)
                  << std::flush;
    }

    bool copy_file(
        const fs::path& source,
        const fs::path& destination,
        std::uint64_t total)
    {
        std::ifstream input(source, std::ios::binary);

        if (!input)
        {
            nexo::print_error("Cannot open source file.");
            return false;
        }

        std::error_code ec;

        if (fs::exists(destination, ec) || ec)
        {
            nexo::print_error(
                ec ? "Cannot inspect destination."
                   : "Destination already exists.");
            return false;
        }

        std::ofstream output(
            destination,
            std::ios::binary | std::ios::out);

        if (!output)
        {
            nexo::print_error("Cannot create destination file.");
            return false;
        }

        constexpr std::size_t buffer_size = 1024 * 1024;
        char buffer[buffer_size];
        std::uint64_t transferred = 0;

        print_progress(0, total);

        while (input)
        {
            input.read(
                buffer,
                static_cast<std::streamsize>(buffer_size));

            const std::streamsize count = input.gcount();

            if (count > 0)
            {
                output.write(buffer, count);

                if (!output)
                {
                    output.close();
                    fs::remove(destination, ec);
                    nexo::print_error("Failed to write destination file.");
                    return false;
                }

                transferred += static_cast<std::uint64_t>(count);
                print_progress(transferred, total);
            }
        }

        if (input.bad())
        {
            output.close();
            fs::remove(destination, ec);
            nexo::print_error("Failed to read source file.");
            return false;
        }

        output.close();

        if (!output)
        {
            fs::remove(destination, ec);
            nexo::print_error("Failed to finish writing destination file.");
            return false;
        }

        std::cout << '\n';
        return true;
    }
}

namespace nexo::commands
{
    int copy(int argc, char* argv[])
    {
        if (argc != 3)
        {
            print_usage();
            return 2;
        }

        const fs::path source{argv[1]};
        const fs::path destination{argv[2]};

        std::error_code ec;

        if (!fs::exists(source, ec) || ec)
        {
            nexo::print_error("Source does not exist or cannot be accessed.");
            return 1;
        }

        if (!fs::is_regular_file(source, ec) || ec)
        {
            nexo::print_error("Source must be a regular file.");
            return 1;
        }

        if (fs::exists(destination, ec) || ec)
        {
            nexo::print_error(
                ec ? "Cannot inspect destination."
                   : "Destination already exists.");
            return 1;
        }

        const auto total = fs::file_size(source, ec);

        if (ec)
        {
            nexo::print_error("Cannot determine source file size.");
            return 1;
        }

        std::error_code absolute_error;
        const auto absolute_source = fs::absolute(source, absolute_error);
        const auto display_source =
            absolute_error ? source.string() : absolute_source.string();

        absolute_error.clear();
        const auto absolute_destination =
            fs::absolute(destination, absolute_error);
        const auto display_destination =
            absolute_error ? destination.string()
                           : absolute_destination.string();

        print_header();

        std::cout
            << "  " << gray << "Source       " << reset
            << white << display_source << reset << '\n'
            << "  " << gray << "Destination  " << reset
            << white << display_destination << reset << '\n'
            << "  " << gray << "Size         " << reset
            << format_size(total) << "\n\n"
            << "  Copying...\n\n";

        if (!copy_file(source, destination, total))
        {
            return 1;
        }

        std::cout
            << "  " << green << "Copy completed successfully."
            << reset << "\n\n";

        return 0;
    }
}
