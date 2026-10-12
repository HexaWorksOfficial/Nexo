#include "nexo/network/tls.hpp"

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>

namespace nexo::network
{
    namespace
    {
        std::string describe_tls_error(
            const char* operation,
            int ssl_error,
            int system_error)
        {
            if (ssl_error == SSL_ERROR_WANT_READ)
            {
                return std::string(operation) +
                    ": TLS requires more data from the peer.";
            }

            if (ssl_error == SSL_ERROR_WANT_WRITE)
            {
                return std::string(operation) +
                    ": TLS requires the connection to become writable.";
            }

            if (ssl_error == SSL_ERROR_ZERO_RETURN)
            {
                return std::string(operation) +
                    ": the TLS connection was closed by the peer.";
            }

            if (ssl_error == SSL_ERROR_SYSCALL && system_error != 0)
            {
                return std::string(operation) + ": " +
                    std::strerror(system_error);
            }

            const unsigned long code = ERR_get_error();

            if (code != 0)
            {
                char message[256]{};
                ERR_error_string_n(code, message, sizeof(message));

                return std::string(operation) + ": " + message;
            }

            if (ssl_error == SSL_ERROR_SYSCALL)
            {
                return std::string(operation) +
                    ": unexpected end of connection.";
            }

            return std::string(operation) +
                ": TLS operation failed.";
        }

        bool is_ip_address(const std::string& hostname)
        {
            in_addr ipv4{};
            in6_addr ipv6{};

            return inet_pton(AF_INET, hostname.c_str(), &ipv4) == 1 ||
                inet_pton(AF_INET6, hostname.c_str(), &ipv6) == 1;
        }
    }

    TlsConnection::TlsConnection(
        SSL_CTX* context,
        SSL* session) noexcept
        : context_(context),
          session_(session)
    {
    }

    TlsConnection::~TlsConnection()
    {
        SSL_free(session_);
        SSL_CTX_free(context_);
    }

    TlsConnection::TlsConnection(TlsConnection&& other) noexcept
        : context_(std::exchange(other.context_, nullptr)),
          session_(std::exchange(other.session_, nullptr))
    {
    }

    TlsConnection& TlsConnection::operator=(
        TlsConnection&& other) noexcept
    {
        if (this != &other)
        {
            SSL_free(session_);
            SSL_CTX_free(context_);

            context_ = std::exchange(other.context_, nullptr);
            session_ = std::exchange(other.session_, nullptr);
        }

        return *this;
    }

    int TlsConnection::write(
        const char* data,
        std::size_t size,
        std::string& error)
    {
        error.clear();
        ERR_clear_error();
        errno = 0;

        std::size_t written = 0;

        const int result = SSL_write_ex(
            session_,
            data,
            size,
            &written);

        if (result == 1)
        {
            if (written > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()))
            {
                error = "TLS write exceeded the supported size.";
                return -1;
            }

            return static_cast<int>(written);
        }

        const int system_error = errno;
        const int ssl_error = SSL_get_error(session_, result);

        error = describe_tls_error(
            "TLS write failed",
            ssl_error,
            system_error);

        return -1;
    }

    int TlsConnection::read(
        char* buffer,
        std::size_t size,
        std::string& error)
    {
        error.clear();
        ERR_clear_error();
        errno = 0;

        std::size_t received = 0;

        const int result = SSL_read_ex(
            session_,
            buffer,
            size,
            &received);

        if (result == 1)
        {
            if (received > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()))
            {
                error = "TLS read exceeded the supported size.";
                return -1;
            }

            return static_cast<int>(received);
        }

        const int system_error = errno;
        const int ssl_error = SSL_get_error(session_, result);

        if (ssl_error == SSL_ERROR_ZERO_RETURN)
        {
            return 0;
        }

        error = describe_tls_error(
            "TLS read failed",
            ssl_error,
            system_error);

        return -1;
    }

    std::unique_ptr<TlsConnection> connect_tls(
        int descriptor,
        const std::string& hostname,
        std::string& error)
    {
        error.clear();
        ERR_clear_error();

        SSL_CTX* context = SSL_CTX_new(TLS_client_method());

        if (context == nullptr)
        {
            error = describe_tls_error(
                "Failed to create TLS context",
                SSL_ERROR_SSL,
                0);
            return nullptr;
        }

        if (SSL_CTX_set_min_proto_version(
                context,
                TLS1_2_VERSION) != 1)
        {
            error = describe_tls_error(
                "Failed to configure the minimum TLS version",
                SSL_ERROR_SSL,
                0);
            SSL_CTX_free(context);
            return nullptr;
        }

        SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);

        if (SSL_CTX_set_default_verify_paths(context) != 1)
        {
            error = describe_tls_error(
                "Failed to load trusted certificate authorities",
                SSL_ERROR_SSL,
                0);
            SSL_CTX_free(context);
            return nullptr;
        }

        SSL* session = SSL_new(context);

        if (session == nullptr)
        {
            error = describe_tls_error(
                "Failed to create TLS session",
                SSL_ERROR_SSL,
                0);
            SSL_CTX_free(context);
            return nullptr;
        }

        if (SSL_set_fd(session, descriptor) != 1)
        {
            error = describe_tls_error(
                "Failed to associate TLS with the socket",
                SSL_ERROR_SSL,
                0);
            SSL_free(session);
            SSL_CTX_free(context);
            return nullptr;
        }

        if (is_ip_address(hostname))
        {
            if (X509_VERIFY_PARAM_set1_ip_asc(
                    SSL_get0_param(session),
                    hostname.c_str()) != 1)
            {
                error = describe_tls_error(
                    "Failed to configure IP address verification",
                    SSL_ERROR_SSL,
                    0);
                SSL_free(session);
                SSL_CTX_free(context);
                return nullptr;
            }
        }
        else
        {
            if (SSL_set_tlsext_host_name(
                    session,
                    hostname.c_str()) != 1 ||
                SSL_set1_host(session, hostname.c_str()) != 1)
            {
                error = describe_tls_error(
                    "Failed to configure hostname verification",
                    SSL_ERROR_SSL,
                    0);
                SSL_free(session);
                SSL_CTX_free(context);
                return nullptr;
            }
        }

        errno = 0;
        const int result = SSL_connect(session);

        if (result != 1)
        {
            const int system_error = errno;
            const int ssl_error = SSL_get_error(session, result);

            error = describe_tls_error(
                "TLS handshake failed",
                ssl_error,
                system_error);

            SSL_free(session);
            SSL_CTX_free(context);
            return nullptr;
        }

        const long verification_result = SSL_get_verify_result(session);

        if (verification_result != X509_V_OK)
        {
            const char* message = X509_verify_cert_error_string(
                verification_result);

            error = std::string("TLS certificate verification failed: ") +
                message;

            SSL_free(session);
            SSL_CTX_free(context);
            return nullptr;
        }

        return std::unique_ptr<TlsConnection>(
            new TlsConnection(context, session));
    }
}