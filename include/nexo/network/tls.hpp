
#pragma once

#include <openssl/ssl.h>

#include <cstddef>
#include <memory>
#include <string>

namespace nexo::network
{
    class TlsConnection
    {
    public:
        ~TlsConnection();

        TlsConnection(const TlsConnection&) = delete;
        TlsConnection& operator=(const TlsConnection&) = delete;

        TlsConnection(TlsConnection&& other) noexcept;
        TlsConnection& operator=(TlsConnection&& other) noexcept;

        int write(
            const char* data,
            std::size_t size,
            std::string& error);

        int read(
            char* buffer,
            std::size_t size,
            std::string& error);

    private:
        friend std::unique_ptr<TlsConnection> connect_tls(
            int descriptor,
            const std::string& hostname,
            std::string& error);

        TlsConnection(
            SSL_CTX* context,
            SSL* session) noexcept;

        SSL_CTX* context_;
        SSL* session_;
    };

    std::unique_ptr<TlsConnection> connect_tls(
        int descriptor,
        const std::string& hostname,
        std::string& error);
}
