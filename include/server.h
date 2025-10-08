#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
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
    std::list<connection> connections; // Stores all connections file descriptors for proper shutdown
    std::mutex connections_mutex;
    int max_connections;
    bool running = false;

    bool use_tls = false;
    SSL_CTX* ssl_ctx;

    std::map<std::string, std::vector<char>> static_files;
    std::list<std::unique_ptr<Controller>> controllers;

    void start_server_loop();
    void handle_client(int socket_fd, std::unique_ptr<sockaddr_in> address, std::list<connection>::iterator it);
    void handle_tls_client(SSL* ssl, int socket_fd, std::unique_ptr<sockaddr_in> address, std::list<connection>::iterator it);
public:
    Server(const std::string& host, uint16_t port);
    ~Server();
    void listen_for_clients(int max = 100);
    void use_static_files(const std::string& dir = "wwwroot");

    template <typename... Types>
    void use_controllers() {
        controllers.clear();
        (add_controller<Types>(), ...);
    }

    template <typename T>
    void add_controller() {
        static_assert(std::is_base_of<Controller, T>::value,
                      "Template parameter must be derived from the Controller class");

        std::unique_ptr<T> controller = std::make_unique<T>();
        controllers.push_back(std::move(controller));
    }

    void use_https(const std::string& cert_file, const std::string& key_file);
    void terminate();
};

#endif // SERVER_H
