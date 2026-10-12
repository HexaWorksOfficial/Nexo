#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include "nexo/network/download.hpp"
#include "nexo/network/url.hpp"

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
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

    struct DownloadOptions
    {
        std::string url;
        std::filesystem::path destination;
        bool output_specified = false;
        bool help_requested = false;
    };

    void print_header()
    {
        std::cout
            << '\n'
            << red << bold << "  NEXO" << reset
            << gray << "  // DOWNLOAD" << reset
            << "\n\n";
    }

    void print_usage()
    {
        std::cout
            << '\n'
            << white << bold << "  USAGE" << reset << '\n'
            << "    nexo download [options] <URL>\n\n"
            << white << bold << "  OPTIONS" << reset << '\n'
            << "    -o, --output <path>  Output file path\n"
            << "    -h, --help           Show this help message\n\n";
    }

    bool parse_arguments(
        int argc,
        char* argv[],
        DownloadOptions& options)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "-h" || argument == "--help")
            {
                options.help_requested = true;
                continue;
            }

            if (argument == "-o" || argument == "--output")
            {
                if (options.output_specified)
                {
                    nexo::print_error(
                        "The output path was specified more than once.");
                    return false;
                }

                if (i + 1 >= argc)
                {
                    nexo::print_error(
                        "Please specify a path after " +
                        std::string(argument) + ".");
                    return false;
                }

                const std::string_view output = argv[++i];

                if (output.empty() || output.front() == '-')
                {
                    nexo::print_error("The output path is invalid.");
                    return false;
                }

                options.destination = output;
                options.output_specified = true;
                continue;
            }

            if (!argument.empty() && argument.front() == '-')
            {
                nexo::print_error(
                    "Unknown option: " + std::string(argument));
                return false;
            }

            if (!options.url.empty())
            {
                nexo::print_error(
                    "Only one URL can be downloaded at a time.");
                return false;
            }

            options.url = argument;
        }

        if (options.help_requested)
        {
            return true;
        }

        if (options.url.empty())
        {
            nexo::print_error("Please specify a URL to download.");
            return false;
        }

        return true;
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

    bool determine_destination(
        const nexo::network::Url& url,
        DownloadOptions& options)
    {
        if (options.output_specified)
        {
            return true;
        }

        const std::size_t separator = url.path.find_last_of('/');
        const std::string filename =
            separator == std::string::npos
                ? url.path
                : url.path.substr(separator + 1);

        if (filename.empty() || filename == "." || filename == "..")
        {
            nexo::print_error(
                "Could not determine the filename from the URL. "
                "Use --output to specify one.");
            return false;
        }

        options.destination = filename;
        return true;
    }
}

namespace nexo::commands
{
    int download(int argc, char* argv[])
    {
        DownloadOptions options;

        if (!parse_arguments(argc, argv, options))
        {
            print_usage();
            return 1;
        }

        if (options.help_requested)
        {
            print_usage();
            return 0;
        }

        const auto url = nexo::network::parse_url(options.url);

        if (!url)
        {
            nexo::print_error(
                "Invalid URL. Please use an HTTP or HTTPS URL.");
            return 1;
        }

        if (!determine_destination(*url, options))
        {
            return 1;
        }

        std::error_code filesystem_error;
        const auto absolute_destination =
            std::filesystem::absolute(
                options.destination,
                filesystem_error);

        print_header();

        std::cout
            << "  " << gray << "URL          " << reset
            << white << options.url << reset << '\n'
            << "  " << gray << "Destination  " << reset
            << (filesystem_error
                    ? options.destination.string()
                    : absolute_destination.string())
            << "\n\n"
            << "  Downloading...\n\n";

        std::string error;

        const auto progress =
            [](std::uint64_t downloaded, std::uint64_t total)
        {
            constexpr std::size_t bar_width = 30;

            std::cout << '\r' << "\033[K" << "  ";

            if (total > 0)
            {
                const double ratio = downloaded >= total
                    ? 1.0
                    : static_cast<double>(downloaded) /
                          static_cast<double>(total);

                const auto filled = static_cast<std::size_t>(
                    ratio * static_cast<double>(bar_width));

                std::cout << white << '[' << reset;

                for (std::size_t i = 0; i < bar_width; ++i)
                {
                    if (ratio >= 1.0 || i < filled)
                    {
                        std::cout << green << '=';
                    }
                    else if (i == filled)
                    {
                        std::cout << yellow << '>';
                    }
                    else
                    {
                        std::cout << reset << '-';
                    }
                }

                std::cout << reset << white << ']';
                std::cout << reset;

                std::cout
                    << ' '
                    << std::fixed << std::setprecision(1)
                    << ratio * 100.0 << "%  "
                    << format_size(downloaded)
                    << " / " << format_size(total);
            }
            else
            {
                std::cout
                    << "Downloaded "
                    << format_size(downloaded);
            }

            std::cout << std::flush;
        };

        if (!nexo::network::download_to_file(
                *url,
                options.destination.string(),
                error,
                progress))
        {
            std::cout << '\n';
            nexo::print_error(error);
            return 1;
        }

        std::cout << '\n';

        std::cout
            << "  " << green << "Download completed successfully."
            << reset << '\n'
            << '\n';

        return 0;
    }
}