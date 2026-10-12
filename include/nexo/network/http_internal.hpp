#pragma once

#include "nexo/network/http.hpp"

namespace nexo::network
{
    std::optional<HttpResponse> http_get_once(
        const Url& url,
        std::string& error);
}