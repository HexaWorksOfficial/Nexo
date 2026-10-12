#include "nexo/network/http.hpp"
#include "nexo/network/http_internal.hpp"
#include "nexo/network/stream.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nexo::network
{
    namespace
    {
        constexpr std::size_t max_header_size = 64 * 1024;
        constexpr std::size_t max_body_size = 64 * 1024 * 1024;

        std::string_view trim(std::string_view value)
        {
            while (!value.empty() &&
                   std::isspace(static_cast<unsigned char>(value.front())))
            {
                value.remove_prefix(1);
            }

            while (!value.empty() &&
                   std::isspace(static_cast<unsigned char>(value.back())))
            {
                value.remove_suffix(1);
            }

            return value;
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
                    return static_cast<char>(std::tolower(character));
                });

            return result;
        }

        bool parse_status_line(
            std::string_view line,
            int& status_code)
        {
            const std::size_t separator = line.find(' ');

            if (separator == std::string_view::npos)
            {
                return false;
            }

            line.remove_prefix(separator);

            while (!line.empty() && line.front() == ' ')
            {
                line.remove_prefix(1);
            }

            const std::size_t end = line.find(' ');
            const std::string_view code = line.substr(0, end);

            if (code.size() != 3)
            {
                return false;
            }

            const auto result = std::from_chars(
                code.data(),
                code.data() + code.size(),
                status_code);

            return result.ec == std::errc{} &&
                result.ptr == code.data() + code.size() &&
                status_code >= 100 &&
                status_code <= 599;
        }

        bool decode_chunked(
            std::string_view input,
            std::string& output,
            std::string& error)
        {
            output.clear();

            std::size_t position = 0;

            while (true)
            {
                const std::size_t line_end = input.find("\r\n", position);

                if (line_end == std::string_view::npos)
                {
                    error = "Invalid chunked response: missing chunk size.";
                    return false;
                }

                std::string_view size_text = input.substr(
                    position,
                    line_end - position);

                const std::size_t extension = size_text.find(';');

                if (extension != std::string_view::npos)
                {
                    size_text = size_text.substr(0, extension);
                }

                size_text = trim(size_text);

                if (size_text.empty())
                {
                    error = "Invalid chunked response: empty chunk size.";
                    return false;
                }

                std::size_t chunk_size = 0;

                const auto result = std::from_chars(
                    size_text.data(),
                    size_text.data() + size_text.size(),
                    chunk_size,
                    16);

                if (result.ec != std::errc{} ||
                    result.ptr != size_text.data() + size_text.size())
                {
                    error = "Invalid chunked response: invalid chunk size.";
                    return false;
                }

                position = line_end + 2;

                if (chunk_size == 0)
                {
                    while (true)
                    {
                        const std::size_t trailer_end = input.find(
                            "\r\n",
                            position);

                        if (trailer_end == std::string_view::npos)
                        {
                            error = "Invalid chunked response: incomplete trailers.";
                            return false;
                        }

                        if (trailer_end == position)
                        {
                            return true;
                        }

                        const std::string_view trailer = input.substr(
                            position,
                            trailer_end - position);

                        if (trailer.find(':') == std::string_view::npos)
                        {
                            error = "Invalid chunked response: malformed trailer.";
                            return false;
                        }

                        position = trailer_end + 2;
                    }
                }

                if (chunk_size > max_body_size - output.size())
                {
                    error = "Response body exceeds the 64 MiB limit.";
                    return false;
                }

                if (position > input.size() ||
                    chunk_size > input.size() - position ||
                    input.size() - position - chunk_size < 2)
                {
                    error = "Invalid chunked response: incomplete chunk.";
                    return false;
                }

                output.append(input.data() + position, chunk_size);
                position += chunk_size;

                if (input.substr(position, 2) != "\r\n")
                {
                    error = "Invalid chunked response: missing chunk terminator.";
                    return false;
                }

                position += 2;
            }
        }

        bool write_all(
            NetworkStream& stream,
            std::string_view data,
            std::string& error)
        {
            std::size_t position = 0;

            while (position < data.size())
            {
                const int written = stream.write(
                    data.data() + position,
                    data.size() - position,
                    error);

                if (written <= 0)
                {
                    if (error.empty())
                    {
                        error = "Connection closed while sending the request.";
                    }

                    return false;
                }

                position += static_cast<std::size_t>(written);
            }

            return true;
        }
    }

    std::optional<HttpResponse> http_get_once(
        const Url& url,
        std::string& error)
    {
        error.clear();

        if (url.scheme != "http" && url.scheme != "https")
        {
            error = "Unsupported URL scheme.";
            return std::nullopt;
        }

        auto stream = connect_stream(url, error);

        if (!stream)
        {
            return std::nullopt;
        }

        std::string host = url.host;

        if (host.find(':') != std::string::npos)
        {
            host = "[" + host + "]";
        }

        host += ":" + std::to_string(url.port);

        const std::string request =
            "GET " + url.path + " HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "User-Agent: Nexo/0.1.0\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n";

        if (!write_all(*stream, request, error))
        {
            error = "Failed to send HTTP request: " + error;
            return std::nullopt;
        }

        std::string raw_response;
        std::array<char, 8192> buffer{};

        constexpr std::size_t max_response_size =
            max_header_size + max_body_size;

        while (true)
        {
            const int received = stream->read(
                buffer.data(),
                buffer.size(),
                error);

            if (received < 0)
            {
                error = "Failed to read HTTP response: " + error;
                return std::nullopt;
            }

            if (received == 0)
            {
                break;
            }

            const std::size_t amount =
                static_cast<std::size_t>(received);

            if (amount > max_response_size - raw_response.size())
            {
                error = "HTTP response exceeds the size limit.";
                return std::nullopt;
            }

            raw_response.append(buffer.data(), amount);
        }

        const std::size_t header_end = raw_response.find("\r\n\r\n");

        if (header_end == std::string::npos)
        {
            error = "Invalid HTTP response: missing header terminator.";
            return std::nullopt;
        }

        if (header_end + 4 > max_header_size)
        {
            error = "HTTP headers exceed the 64 KiB limit.";
            return std::nullopt;
        }

        const std::string_view header_block(
            raw_response.data(),
            header_end);

        const std::size_t status_end = header_block.find("\r\n");

        const std::string_view status_line =
            header_block.substr(0, status_end);

        int status_code = 0;

        if (!parse_status_line(status_line, status_code))
        {
            error = "Invalid HTTP status line.";
            return std::nullopt;
        }

        HttpResponse response{
            status_code,
            std::string(status_line),
            {},
            {}
        };

        std::optional<std::size_t> content_length;
        std::optional<std::string> transfer_encoding;

        std::size_t position =
            status_end == std::string_view::npos
                ? header_block.size()
                : status_end + 2;

        while (position < header_block.size())
        {
            const std::size_t line_end = header_block.find(
                "\r\n",
                position);

            const std::size_t end =
                line_end == std::string_view::npos
                    ? header_block.size()
                    : line_end;

            const std::string_view line = header_block.substr(
                position,
                end - position);

            const std::size_t colon = line.find(':');

            if (colon == std::string_view::npos || colon == 0)
            {
                error = "Invalid HTTP header.";
                return std::nullopt;
            }

            const std::string name = lowercase(trim(line.substr(0, colon)));
            const std::string value(trim(line.substr(colon + 1)));

            if (name == "content-length")
            {
                std::size_t parsed_length = 0;

                const auto parsed = std::from_chars(
                    value.data(),
                    value.data() + value.size(),
                    parsed_length);

                if (value.empty() ||
                    parsed.ec != std::errc{} ||
                    parsed.ptr != value.data() + value.size())
                {
                    error = "Invalid Content-Length header.";
                    return std::nullopt;
                }

                if (content_length && *content_length != parsed_length)
                {
                    error = "Conflicting Content-Length headers.";
                    return std::nullopt;
                }

                content_length = parsed_length;
            }
            else if (name == "transfer-encoding")
            {
                if (transfer_encoding)
                {
                    error = "Multiple Transfer-Encoding headers are unsupported.";
                    return std::nullopt;
                }

                transfer_encoding = lowercase(value);
            }

            response.headers.emplace_back(name, value);

            if (line_end == std::string_view::npos)
            {
                break;
            }

            position = line_end + 2;
        }

        const std::string_view body(
            raw_response.data() + header_end + 4,
            raw_response.size() - header_end - 4);

        if (transfer_encoding && content_length)
        {
            error = "HTTP response contains both Transfer-Encoding and Content-Length.";
            return std::nullopt;
        }

        if (transfer_encoding)
        {
            if (*transfer_encoding != "chunked")
            {
                error = "Unsupported Transfer-Encoding.";
                return std::nullopt;
            }

            if (!decode_chunked(body, response.body, error))
            {
                return std::nullopt;
            }
        }
        else
        {
            if (body.size() > max_body_size)
            {
                error = "Response body exceeds the 64 MiB limit.";
                return std::nullopt;
            }

            if (content_length && *content_length != body.size())
            {
                error = "HTTP Content-Length does not match the received body.";
                return std::nullopt;
            }

            response.body.assign(body);
        }

        return response;
    }
}