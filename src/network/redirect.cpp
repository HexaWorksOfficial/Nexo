#include "nexo/network/http.hpp"
#include "nexo/network/http_internal.hpp"
#include "nexo/network/url.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nexo::network
{
    namespace
    {
        constexpr int max_redirects = 10;

        bool is_redirect(int status_code)
        {
            return status_code == 301 ||
                status_code == 302 ||
                status_code == 303 ||
                status_code == 307 ||
                status_code == 308;
        }

        std::optional<std::string> find_location(
            const HttpResponse& response)
        {
            for (const auto& [name, value] : response.headers)
            {
                if (name == "location")
                {
                    return value;
                }
            }

            return std::nullopt;
        }
    }

    std::optional<HttpResponse> http_get(
        const Url& url,
        std::string& error)
    {
        error.clear();

        Url current = url;
        int redirects = 0;

        while (true)
        {
            auto response = http_get_once(current, error);

            if (!response)
            {
                return std::nullopt;
            }

            if (!is_redirect(response->status_code))
            {
                return response;
            }

            const auto location = find_location(*response);

            if (!location)
            {
                return response;
            }

            if (redirects >= max_redirects)
            {
                error = "Too many HTTP redirects.";
                return std::nullopt;
            }

            const auto next = resolve_url(current, *location);

            if (!next)
            {
                error = "Invalid URL in HTTP Location header.";
                return std::nullopt;
            }

            if (current.secure() && !next->secure())
            {
                error = "Refusing to redirect from HTTPS to HTTP.";
                return std::nullopt;
            }

            current = *next;
            ++redirects;
        }
    }
}
