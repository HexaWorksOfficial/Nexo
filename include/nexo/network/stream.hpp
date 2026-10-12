
#pragma once

#include "nexo/network/socket.hpp"
#include "nexo/network/tls.hpp"
#include "nexo/network/url.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace nexo::network
{
    class NetworkStream
    {
    public:
        NetworkStream(const NetworkStream&) = delete;
        NetworkStream& operator=(const NetworkStream&) = delete;

        NetworkStream(NetworkStream&&) noexcept = default;
        NetworkStream& operator=(NetworkStream&&) noexcept = default;

        int write(
            const char* data,
            std::size_t size,
            std::string& error);

        int read(
            char* buffer,
            std::size_t size,
            std::string& error);

    private:
        friend std::unique_ptr<NetworkStream> connect_stream(
            const Url& url,
            std::string& error);

        explicit NetworkStream(
            std::unique_ptr<TcpSocket> socket);

        std::unique_ptr<TcpSocket> socket_;
        std::unique_ptr<TlsConnection> tls_;
    };

    std::unique_ptr<NetworkStream> connect_stream(
        const Url& url,
        std::string& error);
}
