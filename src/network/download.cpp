#include "nexo/network/download.hpp"

#include "nexo/network/stream.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace nexo::network
{
    namespace
    {
        constexpr std::size_t max_header_size = 64 * 1024;
        constexpr std::size_t buffer_size = 16 * 1024;
        constexpr int max_redirects = 10;

        std::string trim(std::string_view value)
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

            return std::string(value);
        }

        std::string lowercase(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });

            return value;
        }

        class BufferedReader
        {
        public:
            explicit BufferedReader(NetworkStream& stream)
                : stream_(stream)
            {
            }

            bool read_line(
                std::string& line,
                std::size_t& header_bytes,
                std::string& error)
            {
                line.clear();

                while (true)
                {
                    char character = '\0';
                    const int result = read_some(&character, 1, error);

                    if (result < 0)
                    {
                        return false;
                    }

                    if (result == 0)
                    {
                        error = "Unexpected end of connection while reading HTTP headers.";
                        return false;
                    }

                    ++header_bytes;

                    if (header_bytes > max_header_size)
                    {
                        error = "HTTP headers exceed the 64 KiB limit.";
                        return false;
                    }

                    if (character == '\n')
                    {
                        if (line.empty() || line.back() != '\r')
                        {
                            error = "Invalid HTTP line ending.";
                            return false;
                        }

                        line.pop_back();
                        return true;
                    }

                    line.push_back(character);

                    if (line.size() > max_header_size)
                    {
                        error = "HTTP line exceeds the size limit.";
                        return false;
                    }
                }
            }

            int read_some(
                char* output,
                std::size_t capacity,
                std::string& error)
            {
                if (capacity == 0)
                {
                    return 0;
                }

                if (position_ < buffered_size_)
                {
                    const std::size_t available =
                        buffered_size_ - position_;

                    const std::size_t count =
                        std::min(available, capacity);

                    std::memcpy(
                        output,
                        buffer_.data() + position_,
                        count);

                    position_ += count;
                    return static_cast<int>(count);
                }

                position_ = 0;
                buffered_size_ = 0;

                const int result = stream_.read(
                    buffer_.data(),
                    buffer_.size(),
                    error);

                if (result <= 0)
                {
                    return result;
                }

                buffered_size_ = static_cast<std::size_t>(result);

                const std::size_t count =
                    std::min(buffered_size_, capacity);

                std::memcpy(output, buffer_.data(), count);
                position_ = count;

                return static_cast<int>(count);
            }

        private:
            NetworkStream& stream_;
            std::array<char, buffer_size> buffer_{};
            std::size_t position_ = 0;
            std::size_t buffered_size_ = 0;
        };

        bool write_all(
            int descriptor,
            const char* data,
            std::size_t size,
            std::string& error)
        {
            std::size_t written = 0;

            while (written < size)
            {
                const ssize_t result = ::write(
                    descriptor,
                    data + written,
                    size - written);

                if (result < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    error = "Failed to write the downloaded file: ";
                    error += std::strerror(errno);
                    return false;
                }

                if (result == 0)
                {
                    error = "The file write made no progress.";
                    return false;
                }

                written += static_cast<std::size_t>(result);
            }

            return true;
        }

        bool copy_exact(
            BufferedReader& reader,
            int descriptor,
            std::uint64_t length,
            std::uint64_t max_bytes,
            std::uint64_t& downloaded,
            const DownloadProgressCallback& progress,
            std::uint64_t total,
            std::string& error)
        {
            if (downloaded > max_bytes ||
                length > max_bytes - downloaded)
            {
                error = "Download exceeds the configured size limit.";
                return false;
            }

            std::array<char, buffer_size> buffer{};
            std::uint64_t remaining = length;

            while (remaining > 0)
            {
                const std::size_t requested =
                    static_cast<std::size_t>(
                        std::min<std::uint64_t>(
                            remaining,
                            buffer.size()));

                const int result = reader.read_some(
                    buffer.data(),
                    requested,
                    error);

                if (result < 0)
                {
                    return false;
                }

                if (result == 0)
                {
                    error = "The connection ended before the download completed.";
                    return false;
                }

                const auto count = static_cast<std::size_t>(result);

                if (!write_all(
                        descriptor,
                        buffer.data(),
                        count,
                        error))
                {
                    return false;
                }

                remaining -= count;
                downloaded += count;

                if (progress)
                {
                    progress(downloaded, total);
                }
            }

            return true;
        }

        bool copy_until_eof(
            BufferedReader& reader,
            int descriptor,
            std::uint64_t max_bytes,
            std::uint64_t& downloaded,
            const DownloadProgressCallback& progress,
            std::string& error)
        {
            std::array<char, buffer_size> buffer{};

            while (true)
            {
                const int result = reader.read_some(
                    buffer.data(),
                    buffer.size(),
                    error);

                if (result < 0)
                {
                    return false;
                }

                if (result == 0)
                {
                    return true;
                }

                const auto count = static_cast<std::size_t>(result);

                if (downloaded > max_bytes ||
                    count > max_bytes - downloaded)
                {
                    error = "Download exceeds the configured size limit.";
                    return false;
                }

                if (!write_all(
                        descriptor,
                        buffer.data(),
                        count,
                        error))
                {
                    return false;
                }

                downloaded += count;

                if (progress)
                {
                    progress(downloaded, 0);
                }
            }
        }

        bool read_chunk_size(
            BufferedReader& reader,
            std::size_t& header_bytes,
            std::uint64_t& chunk_size,
            std::string& error)
        {
            std::string line;

            if (!reader.read_line(line, header_bytes, error))
            {
                return false;
            }

            const auto extension = line.find(';');

            if (extension != std::string::npos)
            {
                line.resize(extension);
            }

            line = trim(line);

            if (line.empty())
            {
                error = "Invalid empty HTTP chunk size.";
                return false;
            }

            std::uint64_t value = 0;

            const char* begin = line.data();
            const char* end = begin + line.size();

            const auto result = std::from_chars(
                begin,
                end,
                value,
                16);

            if (result.ec != std::errc{} || result.ptr != end)
            {
                error = "Invalid HTTP chunk size.";
                return false;
            }

            chunk_size = value;
            return true;
        }

        bool copy_chunked(
            BufferedReader& reader,
            int descriptor,
            std::uint64_t max_bytes,
            std::uint64_t& downloaded,
            const DownloadProgressCallback& progress,
            std::size_t& header_bytes,
            std::string& error)
        {
            while (true)
            {
                std::uint64_t chunk_size = 0;

                if (!read_chunk_size(
                        reader,
                        header_bytes,
                        chunk_size,
                        error))
                {
                    return false;
                }

                if (chunk_size == 0)
                {
                    while (true)
                    {
                        std::string trailer;

                        if (!reader.read_line(
                                trailer,
                                header_bytes,
                                error))
                        {
                            return false;
                        }

                        if (trailer.empty())
                        {
                            return true;
                        }

                        if (trailer.find(':') == std::string::npos)
                        {
                            error = "Invalid HTTP trailer.";
                            return false;
                        }
                    }
                }

                if (!copy_exact(
                        reader,
                        descriptor,
                        chunk_size,
                        max_bytes,
                        downloaded,
                        progress,
                        0,
                        error))
                {
                    return false;
                }

                char ending[2]{};

                const int first = reader.read_some(
                    ending,
                    sizeof(ending),
                    error);

                if (first < 0)
                {
                    return false;
                }

                if (first != 2 ||
                    ending[0] != '\r' ||
                    ending[1] != '\n')
                {
                    error = "Invalid HTTP chunk terminator.";
                    return false;
                }
            }
        }

        bool parse_content_length(
            const std::string& value,
            std::uint64_t& length)
        {
            const std::string cleaned = trim(value);

            if (cleaned.empty())
            {
                return false;
            }

            const char* begin = cleaned.data();
            const char* end = begin + cleaned.size();

            const auto result = std::from_chars(begin, end, length);

            return result.ec == std::errc{} &&
                   result.ptr == end;
        }

        bool is_redirect(int status_code)
        {
            return status_code == 301 ||
                   status_code == 302 ||
                   status_code == 303 ||
                   status_code == 307 ||
                   status_code == 308;
        }

        struct ParsedHeaders
        {
            int status_code = 0;
            std::string status_line;
            std::optional<std::string> location;
            std::optional<std::uint64_t> content_length;
            bool chunked = false;
        };

        bool read_response_headers(
            BufferedReader& reader,
            ParsedHeaders& response,
            std::size_t& header_bytes,
            std::string& error)
        {
            std::string line;

            if (!reader.read_line(line, header_bytes, error))
            {
                return false;
            }

            response.status_line = line;

            if (line.size() < 12 ||
                line.compare(0, 5, "HTTP/") != 0 ||
                line[8] != ' ')
            {
                error = "Invalid HTTP status line.";
                return false;
            }

            const std::string_view status_text(line.data() + 9, 3);

            const auto status_result = std::from_chars(
                status_text.data(),
                status_text.data() + status_text.size(),
                response.status_code);

            if (status_result.ec != std::errc{} ||
                status_result.ptr != status_text.data() + status_text.size())
            {
                error = "Invalid HTTP status code.";
                return false;
            }

            while (true)
            {
                if (!reader.read_line(line, header_bytes, error))
                {
                    return false;
                }

                if (line.empty())
                {
                    return true;
                }

                const auto separator = line.find(':');

                if (separator == std::string::npos || separator == 0)
                {
                    error = "Invalid HTTP header.";
                    return false;
                }

                const std::string name = lowercase(
                    trim(std::string_view(line).substr(0, separator)));

                const std::string value = trim(
                    std::string_view(line).substr(separator + 1));

                if (name == "location")
                {
                    response.location = value;
                }
                else if (name == "content-length")
                {
                    std::uint64_t length = 0;

                    if (!parse_content_length(value, length))
                    {
                        error = "Invalid Content-Length header.";
                        return false;
                    }

                    if (response.content_length &&
                        *response.content_length != length)
                    {
                        error = "Conflicting Content-Length headers.";
                        return false;
                    }

                    response.content_length = length;
                }
                else if (name == "transfer-encoding")
                {
                    if (lowercase(value) != "chunked")
                    {
                        error = "Unsupported HTTP Transfer-Encoding.";
                        return false;
                    }

                    response.chunked = true;
                }
            }
        }

        class TemporaryFile
        {
        public:
            ~TemporaryFile()
            {
                if (descriptor_ >= 0)
                {
                    ::close(descriptor_);
                }

                if (!published_ && !path_.empty())
                {
                    ::unlink(path_.c_str());
                }
            }

            TemporaryFile(const TemporaryFile&) = delete;
            TemporaryFile& operator=(const TemporaryFile&) = delete;

            TemporaryFile() = default;

            bool create(
                const std::string& destination,
                std::string& error)
            {
                path_ = destination + ".nexo-tmp-XXXXXX";

                std::vector<char> writable_path(
                    path_.begin(),
                    path_.end());

                writable_path.push_back('\0');

                descriptor_ = ::mkstemp(writable_path.data());

                if (descriptor_ < 0)
                {
                    error = "Cannot create a temporary download file: ";
                    error += std::strerror(errno);
                    return false;
                }

                path_ = writable_path.data();

                if (::fchmod(descriptor_, S_IRUSR | S_IWUSR) != 0)
                {
                    error = "Cannot set temporary file permissions: ";
                    error += std::strerror(errno);
                    return false;
                }

                return true;
            }

            int descriptor() const noexcept
            {
                return descriptor_;
            }

            bool publish(
                const std::string& destination,
                std::string& error)
            {
                if (::fsync(descriptor_) != 0)
                {
                    error = "Cannot synchronize the downloaded file: ";
                    error += std::strerror(errno);
                    return false;
                }

                if (::close(descriptor_) != 0)
                {
                    descriptor_ = -1;
                    error = "Cannot close the downloaded file: ";
                    error += std::strerror(errno);
                    return false;
                }

                descriptor_ = -1;

                if (::link(path_.c_str(), destination.c_str()) != 0)
                {
                    error = "Cannot publish the downloaded file: ";
                    error += std::strerror(errno);
                    return false;
                }

                published_ = true;

                if (::unlink(path_.c_str()) != 0)
                {
                    error = "The file was downloaded, but its temporary link could not be removed: ";
                    error += std::strerror(errno);
                    return false;
                }

                path_.clear();
                return true;
            }

        private:
            int descriptor_ = -1;
            std::string path_;
            bool published_ = false;
        };
    }

    bool download_to_file(
        const Url& url,
        const std::string& destination,
        std::string& error,
        DownloadProgressCallback progress,
        std::uint64_t max_bytes)
    {
        error.clear();

        if (destination.empty())
        {
            error = "The destination path cannot be empty.";
            return false;
        }

        Url current = url;

        for (int redirect_count = 0;
             redirect_count <= max_redirects;
             ++redirect_count)
        {
            auto stream = connect_stream(current, error);

            if (!stream)
            {
                return false;
            }

            std::string request =
                "GET " + current.path + " HTTP/1.1\r\n"
                "Host: " + current.host;

            const bool standard_port =
                (current.scheme == "http" && current.port == 80) ||
                (current.scheme == "https" && current.port == 443);

            if (!standard_port)
            {
                request += ":" + std::to_string(current.port);
            }

            request +=
                "\r\n"
                "User-Agent: Nexo/0.1.0\r\n"
                "Accept: */*\r\n"
                "Accept-Encoding: identity\r\n"
                "Connection: close\r\n"
                "\r\n";

            std::size_t sent = 0;

            while (sent < request.size())
            {
                const int result = stream->write(
                    request.data() + sent,
                    request.size() - sent,
                    error);

                if (result <= 0)
                {
                    if (result == 0)
                    {
                        error = "Connection closed while sending the HTTP request.";
                    }

                    return false;
                }

                sent += static_cast<std::size_t>(result);
            }

            BufferedReader reader(*stream);
            ParsedHeaders response;
            std::size_t header_bytes = 0;

            if (!read_response_headers(
                    reader,
                    response,
                    header_bytes,
                    error))
            {
                return false;
            }

            if (response.chunked && response.content_length)
            {
                error = "HTTP response contains both Transfer-Encoding and Content-Length.";
                return false;
            }

            if (is_redirect(response.status_code))
            {
                if (!response.location || response.location->empty())
                {
                    error = "HTTP redirect did not provide a Location header.";
                    return false;
                }

                if (redirect_count == max_redirects)
                {
                    error = "Too many HTTP redirects.";
                    return false;
                }

                auto next = resolve_url(current, *response.location);

                if (!next)
                {
                    error = "HTTP redirect contains an invalid URL.";
                    return false;
                }

                if (current.secure() && !next->secure())
                {
                    error = "Refusing an HTTPS-to-HTTP redirect.";
                    return false;
                }

                current = std::move(*next);
                continue;
            }

            if (response.status_code < 200 ||
                response.status_code >= 300)
            {
                error = "HTTP request failed: " + response.status_line;
                return false;
            }

            if (response.content_length &&
                *response.content_length > max_bytes)
            {
                error = "Download exceeds the configured size limit.";
                return false;
            }

            TemporaryFile file;

            if (!file.create(destination, error))
            {
                return false;
            }

            std::uint64_t downloaded = 0;
            bool success = false;

            if (progress)
            {
                progress(
                    0,
                    response.content_length.value_or(0));
            }

            if (response.chunked)
            {
                success = copy_chunked(
                    reader,
                    file.descriptor(),
                    max_bytes,
                    downloaded,
                    progress,
                    header_bytes,
                    error);
            }
            else if (response.content_length)
            {
                success = copy_exact(
                    reader,
                    file.descriptor(),
                    *response.content_length,
                    max_bytes,
                    downloaded,
                    progress,
                    *response.content_length,
                    error);
            }
            else
            {
                success = copy_until_eof(
                    reader,
                    file.descriptor(),
                    max_bytes,
                    downloaded,
                    progress,
                    error);
            }

            if (!success)
            {
                return false;
            }

            return file.publish(destination, error);
        }

        error = "Too many HTTP redirects.";
        return false;
    }

    bool download_to_file(
        const Url& url,
        const std::string& destination,
        std::string& error,
        std::uint64_t max_bytes)
    {
        return download_to_file(
            url,
            destination,
            error,
            DownloadProgressCallback{},
            max_bytes);
    }
}
