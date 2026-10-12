#include "nexo/network/url.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>
#include <system_error>
#include <vector>

namespace nexo::network
{
    namespace
    {
        bool contains_whitespace(std::string_view value)
        {
            return std::any_of(
                value.begin(),
                value.end(),
                [](unsigned char character)
                {
                    return std::isspace(character) != 0;
                });
        }

        std::string lowercase(std::string_view value)
        {
            std::string result(value);

            std::transform(
                result.begin(),
                result.end(),
                result.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::tolower(character));
                });

            return result;
        }

        bool parse_port(
            std::string_view input,
            std::uint16_t& port)
        {
            if (input.empty())
            {
                return false;
            }

            unsigned int value = 0;

            const auto result = std::from_chars(
                input.data(),
                input.data() + input.size(),
                value);

            if (result.ec != std::errc{}
                || result.ptr != input.data() + input.size()
                || value == 0
                || value > 65535)
            {
                return false;
            }

            port = static_cast<std::uint16_t>(value);
            return true;
        }

        std::string authority(const Url& url)
        {
            std::string result;

            if (url.host.find(':') != std::string::npos)
            {
                result = "[" + url.host + "]";
            }
            else
            {
                result = url.host;
            }

            const bool default_port =
                (url.scheme == "http" && url.port == 80) ||
                (url.scheme == "https" && url.port == 443);

            if (!default_port)
            {
                result += ":" + std::to_string(url.port);
            }

            return result;
        }

        std::string normalize_path(std::string_view input)
        {
            const std::size_t query_position = input.find('?');

            const std::string_view pathname = input.substr(
                0,
                query_position);

            const std::string_view query =
                query_position == std::string_view::npos
                    ? std::string_view{}
                    : input.substr(query_position);

            std::vector<std::string_view> segments;
            std::size_t position = 0;

            while (position < pathname.size())
            {
                while (position < pathname.size() &&
                       pathname[position] == '/')
                {
                    ++position;
                }

                const std::size_t end = pathname.find('/', position);

                const std::string_view segment = pathname.substr(
                    position,
                    end == std::string_view::npos
                        ? pathname.size() - position
                        : end - position);

                if (segment == "..")
                {
                    if (!segments.empty())
                    {
                        segments.pop_back();
                    }
                }
                else if (!segment.empty() && segment != ".")
                {
                    segments.push_back(segment);
                }

                if (end == std::string_view::npos)
                {
                    break;
                }

                position = end + 1;
            }

            std::string result = "/";

            for (std::size_t index = 0; index < segments.size(); ++index)
            {
                if (index > 0)
                {
                    result += "/";
                }

                result.append(segments[index]);
            }

            const bool trailing_slash =
                !pathname.empty() && pathname.back() == '/';

            if (trailing_slash &&
                result.back() != '/')
            {
                result += "/";
            }

            result += query;
            return result;
        }
    }

    bool Url::secure() const noexcept
    {
        return scheme == "https";
    }

    std::optional<Url> parse_url(std::string_view input)
    {
        if (input.empty() || contains_whitespace(input))
        {
            return std::nullopt;
        }

        const auto separator = input.find("://");

        if (separator == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::string scheme = lowercase(
            input.substr(0, separator));

        if (scheme != "http" && scheme != "https")
        {
            return std::nullopt;
        }

        input.remove_prefix(separator + 3);

        const auto authority_end = input.find_first_of("/?#");
        const std::string_view host_port = input.substr(
            0,
            authority_end);

        if (host_port.empty()
            || host_port.find('@') != std::string_view::npos)
        {
            return std::nullopt;
        }

        std::string host;
        std::uint16_t port = scheme == "https" ? 443 : 80;

        if (host_port.front() == '[')
        {
            const auto closing_bracket = host_port.find(']');

            if (closing_bracket == std::string_view::npos
                || closing_bracket == 1)
            {
                return std::nullopt;
            }

            host = std::string(
                host_port.substr(1, closing_bracket - 1));

            const auto remainder = host_port.substr(
                closing_bracket + 1);

            if (!remainder.empty())
            {
                if (remainder.front() != ':'
                    || !parse_port(remainder.substr(1), port))
                {
                    return std::nullopt;
                }
            }
        }
        else
        {
            const auto colon = host_port.find(':');

            if (colon != std::string_view::npos)
            {
                if (host_port.find(':', colon + 1)
                    != std::string_view::npos)
                {
                    return std::nullopt;
                }

                host = lowercase(host_port.substr(0, colon));

                if (!parse_port(host_port.substr(colon + 1), port))
                {
                    return std::nullopt;
                }
            }
            else
            {
                host = lowercase(host_port);
            }
        }

        if (host.empty())
        {
            return std::nullopt;
        }

        std::string path = "/";

        if (authority_end != std::string_view::npos)
        {
            const auto suffix = input.substr(authority_end);

            if (suffix.front() == '/')
            {
                path = std::string(suffix);
            }
            else if (suffix.front() == '?')
            {
                path = "/" + std::string(suffix);
            }

            const auto fragment = path.find('#');

            if (fragment != std::string::npos)
            {
                path.erase(fragment);
            }
        }

        return Url{
            scheme,
            host,
            port,
            path
        };
    }

    std::optional<Url> resolve_url(
        const Url& base,
        std::string_view reference)
    {
        if (reference.empty() || contains_whitespace(reference))
        {
            return std::nullopt;
        }

        const std::size_t fragment = reference.find('#');

        if (fragment != std::string_view::npos)
        {
            reference = reference.substr(0, fragment);
        }

        if (reference.empty())
        {
            return base;
        }

        if (reference.starts_with("//"))
        {
            return parse_url(base.scheme + ":" + std::string(reference));
        }

        if (reference.find("://") != std::string_view::npos)
        {
            return parse_url(reference);
        }

        std::string path;

        if (reference.front() == '/')
        {
            path = std::string(reference);
        }
        else if (reference.front() == '?')
        {
            const std::size_t query = base.path.find('?');

            path = base.path.substr(0, query);
            path += reference;
        }
        else if (reference.front() == '#')
        {
            return base;
        }
        else
        {
            const std::size_t query = base.path.find('?');

            const std::string_view base_path(
                base.path.data(),
                query == std::string::npos
                    ? base.path.size()
                    : query);

            const std::size_t slash = base_path.rfind('/');

            if (slash != std::string_view::npos)
            {
                path = std::string(base_path.substr(0, slash + 1));
            }
            else
            {
                path = "/";
            }

            path += reference;
        }

        path = normalize_path(path);

        const std::string absolute =
            base.scheme + "://" + authority(base) + path;

        return parse_url(absolute);
    }
}