#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nexo::network
{
    struct Url
    {
        std::string scheme;
        std::string host;
        std::uint16_t port;
        std::string path;

        bool secure() const noexcept;
    };

    std::optional<Url> parse_url(std::string_view input);

    std::optional<Url> resolve_url(
        const Url& base,
        std::string_view reference);
}