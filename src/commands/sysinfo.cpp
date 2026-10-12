#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>

namespace
{
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* red = "\033[38;2;255;70;70m";
    constexpr const char* gray = "\033[38;2;150;150;150m";
    constexpr const char* white = "\033[38;2;240;240;240m";

    void print_header(const char* subtitle)
    {
        std::cout
            << '\n'
            << red << bold << "  NEXO" << reset
            << gray << "  // " << subtitle << reset
            << "\n\n";
    }

    void print_section(const char* title)
    {
        std::cout
            << "  " << white << bold << title << reset
            << '\n';
    }

    void print_sysinfo_help()
    {
        print_header("SYSTEM INFORMATION");

        std::cout
            << "  A versatile command-line toolkit for Linux.\n"
            << "  Inspect your system using native Linux interfaces.\n\n";

        print_section("USAGE");

        std::cout
            << "    nexo sysinfo [options]\n\n";

        print_section("OPTIONS");

        std::cout
            << "    " << red << "--resume" << reset
            << gray << ", " << reset
            << red << "-r" << reset
            << "       Show a concise system summary\n"

            << "    " << red << "--help" << reset
            << gray << ", " << reset
            << red << "-h" << reset
            << "       Show this help message\n\n";

        print_section("INFORMATION");

        std::cout
            << "    " << gray << "Default" << reset
            << "     Display detailed system information\n"

            << "    " << gray << "Resume" << reset
            << "      Display OS, kernel, architecture and memory\n\n";

        print_section("EXAMPLES");

        std::cout
            << "    nexo sysinfo\n"
            << "    nexo sysinfo --resume\n"
            << "    nexo sysinfo -r\n\n";
    }

    std::string trim(std::string value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
        {
            return {};
        }

        const auto last = value.find_last_not_of(" \t\r\n");

        return value.substr(0, last - first + 1);
    }

    std::string read_file(const std::string& path)
    {
        std::ifstream file(path);
        std::string value;

        if (file && std::getline(file, value))
        {
            return trim(value);
        }

        return {};
    }

    std::string read_os_value(const std::string& key)
    {
        std::ifstream file("/etc/os-release");
        std::string line;
        const std::string prefix = key + "=";

        while (std::getline(file, line))
        {
            if (line.compare(0, prefix.size(), prefix) == 0)
            {
                std::string value = line.substr(prefix.size());

                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\'')))
                {
                    value = value.substr(1, value.size() - 2);
                }

                return value;
            }
        }

        return "Unknown";
    }

    std::string get_hostname()
    {
        char hostname[256]{};

        if (gethostname(hostname, sizeof(hostname) - 1) == 0)
        {
            return hostname;
        }

        return "Unknown";
    }

    std::string get_architecture(const std::string& machine)
    {
        if (machine == "x86_64")
        {
            return "x86_64";
        }

        if (machine == "aarch64" || machine == "arm64")
        {
            return "ARM64";
        }

        if (machine == "i386" || machine == "i486" ||
            machine == "i586" || machine == "i686")
        {
            return "x86";
        }

        return machine.empty() ? "Unknown" : machine;
    }

    std::string get_session()
    {
        const char* type = std::getenv("XDG_SESSION_TYPE");

        if (type && *type)
        {
            std::string session = type;

            std::transform(
                session.begin(), session.end(), session.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });

            return session;
        }

        if (std::getenv("WAYLAND_DISPLAY"))
        {
            return "WAYLAND";
        }

        if (std::getenv("DISPLAY"))
        {
            return "X11";
        }

        return "Unknown";
    }

    std::string get_desktop()
    {
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");

        if (!desktop || !*desktop)
        {
            desktop = std::getenv("XDG_SESSION_DESKTOP");
        }

        return desktop && *desktop ? desktop : "Unknown";
    }

    std::string get_shell()
    {
        const char* shell = std::getenv("SHELL");

        if (!shell || !*shell)
        {
            return "Unknown";
        }

        const std::string path(shell);
        const auto separator = path.find_last_of('/');

        return separator == std::string::npos
            ? path
            : path.substr(separator + 1);
    }

    std::string get_cpu()
    {
        std::ifstream file("/proc/cpuinfo");
        std::string line;

        while (std::getline(file, line))
        {
            const auto separator = line.find(':');

            if (separator == std::string::npos)
            {
                continue;
            }

            const std::string key = trim(line.substr(0, separator));

            if (key == "model name" || key == "Hardware" ||
                key == "Processor")
            {
                return trim(line.substr(separator + 1));
            }
        }

        return "Unknown";
    }

    std::string get_memory()
    {
        std::ifstream file("/proc/meminfo");
        std::string line;
        unsigned long long total = 0;
        unsigned long long available = 0;

        while (std::getline(file, line))
        {
            std::istringstream input(line);
            std::string key;
            unsigned long long value = 0;

            if (!(input >> key >> value))
            {
                continue;
            }

            if (key == "MemTotal:")
            {
                total = value;
            }
            else if (key == "MemAvailable:")
            {
                available = value;
            }
        }

        if (total == 0)
        {
            return "Unknown";
        }

        constexpr double kib_to_gib = 1024.0 * 1024.0;

        std::ostringstream output;
        output << std::fixed << std::setprecision(2)
               << (total / kib_to_gib) << " GiB total";

        if (available > 0)
        {
            output << ", " << (available / kib_to_gib)
                   << " GiB available";
        }

        return output.str();
    }

    std::string get_uptime()
    {
        std::ifstream file("/proc/uptime");
        double seconds = 0.0;

        if (!(file >> seconds))
        {
            return "Unknown";
        }

        const auto total =
            static_cast<unsigned long long>(seconds);

        const auto days = total / 86400;
        const auto hours = (total % 86400) / 3600;
        const auto minutes = (total % 3600) / 60;
        const auto remaining_seconds = total % 60;

        std::ostringstream output;

        if (days > 0)
        {
            output << days << "d ";
        }

        if (days > 0 || hours > 0)
        {
            output << hours << "h ";
        }

        if (days > 0 || hours > 0 || minutes > 0)
        {
            output << minutes << "m ";
        }

        output << remaining_seconds << "s";

        return output.str();
    }

    std::string get_gpu()
    {
        DIR* directory = opendir("/sys/bus/pci/devices");

        if (!directory)
        {
            return "Unknown";
        }

        std::vector<std::string> devices;
        struct dirent* entry;

        while ((entry = readdir(directory)) != nullptr)
        {
            const std::string name(entry->d_name);

            if (name != "." && name != "..")
            {
                devices.push_back(name);
            }
        }

        closedir(directory);

        std::sort(devices.begin(), devices.end());

        std::vector<std::string> gpus;

        for (const auto& device : devices)
        {
            const std::string base =
                "/sys/bus/pci/devices/" + device + "/";

            const std::string class_id = read_file(base + "class");

            if (class_id.rfind("0x03", 0) != 0)
            {
                continue;
            }

            const std::string vendor = read_file(base + "vendor");
            const std::string product = read_file(base + "device");

            std::string manufacturer = "Unknown vendor";

            if (vendor == "0x10de")
            {
                manufacturer = "NVIDIA";
            }
            else if (vendor == "0x8086")
            {
                manufacturer = "Intel";
            }
            else if (vendor == "0x1002" || vendor == "0x1022")
            {
                manufacturer = "AMD";
            }
            else if (vendor == "0x106b")
            {
                manufacturer = "Apple";
            }
            else if (vendor == "0x1013")
            {
                manufacturer = "Cirrus Logic";
            }

            std::string gpu = manufacturer;

            if (!product.empty())
            {
                gpu += " GPU (" + product + ")";
            }
            else
            {
                gpu += " GPU";
            }

            gpus.push_back(gpu);
        }

        if (gpus.empty())
        {
            return "Unknown";
        }

        std::ostringstream output;

        for (std::size_t i = 0; i < gpus.size(); ++i)
        {
            if (i > 0)
            {
                output << "; ";
            }

            output << gpus[i];
        }

        return output.str();
    }

    void print_field(const char* label, const std::string& value)
    {
        std::cout << "    " << gray << std::left << std::setw(15)
                  << label << reset << value << '\n';
    }
}

namespace nexo::commands
{
    int sysinfo(int argc, char* argv[])
    {
        bool resume = false;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "--resume" || argument == "-r")
            {
                resume = true;
            }
            else if (argument == "--help" || argument == "-h")
            {
                print_sysinfo_help();
                return 0;
            }
            else
            {
                nexo::print_error_with_hint(
                    "Unknown sysinfo option: " + std::string(argument),
                    "nexo sysinfo --help");

                return 2;
            }
        }

        struct utsname system{};

        if (uname(&system) != 0)
        {
            nexo::print_error(
                "Unable to retrieve system information.");

            return 1;
        }

        std::string os_name = read_os_value("PRETTY_NAME");

        if (os_name == "Unknown")
        {
            os_name = read_os_value("NAME");
        }

        print_header("SYSTEM INFORMATION");

        print_field("OS", os_name);

        if (resume)
        {
            print_field("KERNEL", std::string(system.release));
            print_field(
                "ARCHITECTURE",
                get_architecture(system.machine));
            print_field("MEMORY", get_memory());
        }
        else
        {
            print_field("HOSTNAME", get_hostname());
            print_field("KERNEL", std::string(system.release));
            print_field("SESSION", get_session());
            print_field(
                "ARCHITECTURE",
                get_architecture(system.machine));
            print_field("DESKTOP", get_desktop());
            print_field("CPU", get_cpu());
            print_field("GPU", get_gpu());
            print_field("MEMORY", get_memory());
            print_field("UPTIME", get_uptime());
            print_field("SHELL", get_shell());
        }

        std::cout << '\n';

        return 0;
    }
}