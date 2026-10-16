# Nexo

**A versatile command-line toolkit for Linux.**

Nexo is an open-source command-line toolkit written in C++ for Linux. It provides practical file management, downloading, and system information commands through a unified terminal interface.

## About

A build of Nexo is tested before being released to users. Testing is performed on [Debian](https://www.debian.org/).

## Features

- **Download** — Download files from URLs.
- **Copy** — Copy files with progress information.
- **Move** — Move files between locations.
- **System information** — Display information about the system.
- **Version** — Display Nexo version information.
- **Help** — Display available commands and usage instructions.

## System Requirements

The initial binary release targets **Linux x86_64 (64-bit)** on Debian, Ubuntu, and compatible distributions that provide the required runtime libraries.

The current build requires:

- A compatible GNU C Library (glibc), providing at least `GLIBC_2.34`.
- A compatible GNU C++ standard library, providing `GLIBCXX_3.4.32`.
- OpenSSL 3 runtime libraries.
- The standard GCC runtime libraries.

The exact compatibility depends on the versions available in your distribution. Older Debian and Ubuntu releases may not satisfy these requirements.

## Download

Download the prebuilt Linux x86_64 executable from the [GitHub Releases](https://github.com/samuelwahbe/Nexo/releases) page.

Choose the asset named `nexo-linux-x86_64`.

You do not need to install a C++ compiler, CMake, or Git to run a prebuilt release.

## Installation

### 1. Install the runtime dependencies

On Debian/Ubuntu, install the OpenSSL runtime library:

```bash
sudo apt update
sudo apt install libssl3t64
```

On other Debian or Ubuntu releases, the OpenSSL runtime package name may differ. Install the package that provides `libssl.so.3` for your distribution.

Make sure your system also provides compatible glibc and libstdc++ versions.

### 2. Download and extract Nexo

Download `nexo-linux-x86_64.tar.gz` from the [GitHub Releases](https://github.com/samuelwahbe/Nexo/releases) page and save it to your Downloads directory.

Extract the archive:

```bash
cd "$HOME/Downloads"
tar -xzf nexo-linux-x86_64.tar.gz
```

### 3. Install the executable

Run:

```bash
sudo install -m 755 \
  "$HOME/Downloads/nexo-0.1.0-linux-x86_64/nexo" \
  /usr/local/bin/nexo
```

This installs the executable in `/usr/local/bin` and sets the appropriate executable permissions.

### 4. Verify the installation

Check the installed version:

```bash
nexo version
```

Display the available commands:

```bash
nexo help
```

If both commands work, Nexo is ready to use.

## Usage

The general command syntax is:

```text
nexo <command> [options]
```

Display the help message:

```bash
nexo help
```

Display version information:

```bash
nexo version
```

Display system information:

```bash
nexo sysinfo
```

Download a file:

```bash
nexo download <URL>
```

Copy a file:

```bash
nexo copy <source> <destination>
```

Move a file:

```bash
nexo move <source> <destination>
```

For command-specific options and additional information, consult the help output provided by the installed version.

## Uninstallation

Remove the installed executable:

```bash
sudo rm /usr/local/bin/nexo
```

This removes Nexo itself. It does not remove the system libraries installed as dependencies.

## License

Nexo is open-source software distributed under the MIT License. See the [LICENSE](LICENSE) file for the complete license text.
