
#pragma once

#include "nexo/network/url.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexo::network
{
    struct HttpResponse
    {
        int status_code;
        std::string status_line;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
    };

    std::optional<HttpResponse> http_get(
        const Url& url,
        std::string& error);
}
