#pragma once

#include "nexo/network/url.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace nexo::network
{
    using DownloadProgressCallback = std::function<void(
        std::uint64_t downloaded,
        std::uint64_t total)>;

    bool download_to_file(
        const Url& url,
        const std::string& destination,
        std::string& error,
        std::uint64_t max_bytes = 4ULL * 1024 * 1024 * 1024);

    bool download_to_file(
        const Url& url,
        const std::string& destination,
        std::string& error,
        DownloadProgressCallback progress,
        std::uint64_t max_bytes = 4ULL * 1024 * 1024 * 1024);
}