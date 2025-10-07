#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <openssl/ssl.h>
#include "controller.h"

struct connection {
    int fd;
    SSL* ssl = nullptr;
};

class Server
{
private:
    int fd;
    sockaddr_in address;
    std::vector<connection> connections; // Stores all connections file descriptors for proper shutdown
    bool running = false;

    bool use_tls = false;
    SSL_CTX* ssl_ctx;

    std::map<std::string, std::vector<char>> static_files;
    std::vector<Controller*> controllers;

    void start_server_loop();
    void handle_client(int socket_fd, std::unique_ptr<sockaddr_in> address);
    void handle_tls_client(SSL* ssl, int socket_fd, std::unique_ptr<sockaddr_in> address);
public:
    Server(const std::string& host, uint16_t port);
    ~Server();
    void listen_for_clients(int max = 100);
    void use_static_files(const std::string& dir = "wwwroot");
    void use_controllers(const std::vector<Controller*>& controllers);
    void use_https(const std::string& cert_file, const std::string& key_file);
    void terminate();
};

#endif // SERVER_H
