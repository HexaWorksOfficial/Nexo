
#include "nexo/network/stream.hpp"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <utility>

namespace nexo::network
{
    NetworkStream::NetworkStream(
        std::unique_ptr<TcpSocket> socket)
        : socket_(std::move(socket))
    {
    }

    int NetworkStream::write(
        const char* data,
        std::size_t size,
        std::string& error)
    {
        error.clear();

        if (tls_)
        {
            return tls_->write(data, size, error);
        }

        const auto result = ::send(
            socket_->native_handle(),
            data,
            size,
            MSG_NOSIGNAL);

        if (result < 0)
        {
            if (errno == EINTR)
            {
                return write(data, size, error);
            }

            error = std::strerror(errno);
            return -1;
        }

        if (result == 0 && size != 0)
        {
            error = "Connection closed while sending data.";
            return -1;
        }

        return static_cast<int>(result);
    }

    int NetworkStream::read(
        char* buffer,
        std::size_t size,
        std::string& error)
    {
        error.clear();

        if (tls_)
        {
            return tls_->read(buffer, size, error);
        }

        while (true)
        {
            const auto result = ::recv(
                socket_->native_handle(),
                buffer,
                size,
                0);

            if (result >= 0)
            {
                return static_cast<int>(result);
            }

            if (errno == EINTR)
            {
                continue;
            }

            error = std::strerror(errno);
            return -1;
        }
    }

    std::unique_ptr<NetworkStream> connect_stream(
        const Url& url,
        std::string& error)
    {
        error.clear();

        auto connected_socket = connect_tcp(url, error);

        if (!connected_socket)
        {
            error = "Connection failed: " + error;
            return nullptr;
        }

        auto socket = std::make_unique<TcpSocket>(
            std::move(*connected_socket));

        auto stream = std::unique_ptr<NetworkStream>(
            new NetworkStream(std::move(socket)));

        if (url.secure())
        {
            stream->tls_ = connect_tls(
                stream->socket_->native_handle(),
                url.host,
                error);

            if (!stream->tls_)
            {
                error = "HTTPS connection failed: " + error;
                return nullptr;
            }
        }

        return stream;
    }
}
