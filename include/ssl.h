#ifndef SSL_H
#define SSL_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

inline void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

inline void cleanup_openssl() {
    EVP_cleanup();
}

inline SSL_CTX* create_context() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        std::cerr << "Unable to create ssl context" << endl;
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

inline void configure_context(SSL_CTX* ctx,
                              const std::string& cert_file,
                              const std::string& key_file) {
    if (SSL_CTX_use_certificate_file(ctx, cert_file.data(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file.data(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

// Return 1 if connection established, otherwise 0
// The ssl pointer is set to ssl_ptr
inline int establish_connection(SSL** ssl_ptr, SSL_CTX* ctx, int fd) {
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        close(fd);
        SSL_free(ssl);
        return 0;
    }

    *ssl_ptr = ssl;

    return 1;
}

#endif // SSL_H
