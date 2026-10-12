#include "nexo/command.hpp"
#include "nexo/error.hpp"

#include <iostream>

namespace
{
    constexpr const char* reset = "\033[0m";
    constexpr const char* pink = "\033[38;2;255;130;190m";
    constexpr const char* white = "\033[38;2;245;245;245m";
}

namespace nexo::commands
{
    int meow(int argc, char* argv[])
    {
        std::cout
            << '\n'
            << pink
            << " /\\_/\\\\\n"
            << "( o.o )\n"
            << " > ^ <\n"
            << reset
            << '\n'
            << white
            << "Nexo says: ";

        if (argc > 1)
        {
            for (int i = 1; i < argc; ++i)
            {
                if (i > 1)
                {
                    std::cout << ' ';
                }

                std::cout << argv[i];
            }

            std::cout << '\n';
        }
        else
        {
            std::cout << "Meow! :3\n";
        }

        std::cout << reset << '\n';

        return 0;
    }
}