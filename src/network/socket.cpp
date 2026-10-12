#include "nexo/network/socket.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>

namespace nexo::network
{
    namespace
    {
        constexpr auto socket_io_timeout = std::chrono::seconds(15);

        bool set_socket_timeouts(int descriptor, std::string& error)
        {
            const auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    socket_io_timeout);

            timeval timeout{};
            timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(
                seconds.count());

            timeout.tv_usec = 0;

            if (::setsockopt(
                    descriptor,
                    SOL_SOCKET,
                    SO_RCVTIMEO,
                    &timeout,
                    sizeof(timeout)) < 0)
            {
                error = "Failed to configure receive timeout: ";
                error += std::strerror(errno);
                return false;
            }

            if (::setsockopt(
                    descriptor,
                    SOL_SOCKET,
                    SO_SNDTIMEO,
                    &timeout,
                    sizeof(timeout)) < 0)
            {
                error = "Failed to configure send timeout: ";
                error += std::strerror(errno);
                return false;
            }

            return true;
        }

        bool wait_for_connection(
            int descriptor,
            std::chrono::steady_clock::time_point deadline,
            std::string& error)
        {
            while (true)
            {
                const auto now = std::chrono::steady_clock::now();

                if (now >= deadline)
                {
                    error = "Connection timed out.";
                    return false;
                }

                const auto remaining =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - now);

                const auto wait_ms = static_cast<int>(
                    remaining.count() + 1);

                pollfd event{};
                event.fd = descriptor;
                event.events = POLLOUT;

                const int result = ::poll(&event, 1, wait_ms);

                if (result < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    error = "Connection polling failed: ";
                    error += std::strerror(errno);
                    return false;
                }

                if (result == 0)
                {
                    error = "Connection timed out.";
                    return false;
                }

                int socket_error = 0;
                socklen_t error_size = sizeof(socket_error);

                if (::getsockopt(
                        descriptor,
                        SOL_SOCKET,
                        SO_ERROR,
                        &socket_error,
                        &error_size) < 0)
                {
                    error = "Failed to check connection status: ";
                    error += std::strerror(errno);
                    return false;
                }

                if (socket_error != 0)
                {
                    error = std::strerror(socket_error);
                    return false;
                }

                return true;
            }
        }
    }

    TcpSocket::TcpSocket(int descriptor) noexcept
        : descriptor_(descriptor)
    {
    }

    TcpSocket::~TcpSocket()
    {
        if (descriptor_ >= 0)
        {
            ::close(descriptor_);
        }
    }

    TcpSocket::TcpSocket(TcpSocket&& other) noexcept
        : descriptor_(std::exchange(other.descriptor_, -1))
    {
    }

    TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
    {
        if (this != &other)
        {
            if (descriptor_ >= 0)
            {
                ::close(descriptor_);
            }

            descriptor_ = std::exchange(other.descriptor_, -1);
        }

        return *this;
    }

    int TcpSocket::native_handle() const noexcept
    {
        return descriptor_;
    }

    std::optional<TcpSocket> connect_tcp(
        const Url& url,
        std::string& error,
        std::chrono::milliseconds timeout)
    {
        error.clear();

        if (timeout <= std::chrono::milliseconds::zero())
        {
            error = "Connection timeout must be greater than zero.";
            return std::nullopt;
        }

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* addresses = nullptr;
        const std::string port = std::to_string(url.port);

        const int lookup_result = ::getaddrinfo(
            url.host.c_str(),
            port.c_str(),
            &hints,
            &addresses);

        if (lookup_result != 0)
        {
            error = ::gai_strerror(lookup_result);
            return std::nullopt;
        }

        int connected_descriptor = -1;

        for (addrinfo* address = addresses;
             address != nullptr;
             address = address->ai_next)
        {
            const auto deadline =
                std::chrono::steady_clock::now() + timeout;

            const int descriptor = ::socket(
                address->ai_family,
                address->ai_socktype,
                address->ai_protocol);

            if (descriptor < 0)
            {
                error = std::strerror(errno);
                continue;
            }

            const int original_flags = ::fcntl(
                descriptor,
                F_GETFL,
                0);

            if (original_flags < 0 ||
                ::fcntl(
                    descriptor,
                    F_SETFL,
                    original_flags | O_NONBLOCK) < 0)
            {
                error = "Failed to configure connection socket: ";
                error += std::strerror(errno);
                ::close(descriptor);
                continue;
            }

            const int connect_result = ::connect(
                descriptor,
                address->ai_addr,
                address->ai_addrlen);

            bool connected = connect_result == 0;

            if (!connected && errno == EINPROGRESS)
            {
                connected = wait_for_connection(
                    descriptor,
                    deadline,
                    error);
            }
            else if (!connected)
            {
                error = std::strerror(errno);
            }

            if (!connected)
            {
                ::close(descriptor);
                continue;
            }

            if (::fcntl(
                    descriptor,
                    F_SETFL,
                    original_flags) < 0)
            {
                error = "Failed to restore socket flags: ";
                error += std::strerror(errno);
                ::close(descriptor);
                continue;
            }

            if (!set_socket_timeouts(descriptor, error))
            {
                ::close(descriptor);
                continue;
            }

            connected_descriptor = descriptor;
            break;
        }

        ::freeaddrinfo(addresses);

        if (connected_descriptor < 0)
        {
            if (error.empty())
            {
                error = "Could not connect to the server.";
            }

            return std::nullopt;
        }

        return TcpSocket(connected_descriptor);
    }
}