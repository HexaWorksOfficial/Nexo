#pragma once

#include "nexo/network/url.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace nexo::network
{
    class TcpSocket
    {
    public:
        explicit TcpSocket(int descriptor) noexcept;
        ~TcpSocket();

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;

        TcpSocket(TcpSocket&& other) noexcept;
        TcpSocket& operator=(TcpSocket&& other) noexcept;

        int native_handle() const noexcept;

    private:
        int descriptor_;
    };

    std::optional<TcpSocket> connect_tcp(
        const Url& url,
        std::string& error,
        std::chrono::milliseconds timeout = std::chrono::seconds(10));
}