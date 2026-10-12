#pragma once

namespace nexo::commands
{
    int help();
    int version();
    int sysinfo(int argc, char* argv[]);
    int download(int argc, char* argv[]);
    int copy(int argc, char* argv[]);
    int move(int argc, char* argv[]);
    int meow(int argc, char* argv[]);
}
