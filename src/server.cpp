#include "server.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

#include <iostream>

#include <map>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "http.hpp"
#include "controller.h"
#include "ssl.h"
#include "helpers.h"

#define BUFFER_SIZE 16384

namespace fs = std::filesystem;

Server::Server(const std::string& host, uint16_t port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set port");
    }

    address.sin_family = AF_INET;
    if ((address.sin_addr.s_addr = inet_addr(host.c_str())) == INADDR_NONE)
        throw std::invalid_argument("Invalid IP address: " + host);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(struct sockaddr))) {
        throw std::runtime_error("Bind failed");
    }

    fd = server_fd;
    this->address = address;
}

Server::~Server() {
    terminate();
    for (auto& c: controllers) delete c;
}

void Server::start_server_loop() {
    int new_socket;
    socklen_t al = sizeof(address);
    int con_index;
    while (running) {
        con_index = -1;
        for (int i = 0; i < connections.size(); i++) {
            if (connections[i].fd == -1) {
                con_index = i;
                break;
            }
        }
        if (con_index == -1) {
            // throw runtime_error("Too many connections");
            usleep(100000); // Sleep for 0.1 second
            continue;
        }
        std::unique_ptr<sockaddr_in> address = std::make_unique<sockaddr_in>();
        if ((new_socket = accept(fd, (struct sockaddr*)address.get(), &al)) < 0) {
            continue;
        }
        connections[con_index].fd = new_socket;
        if (use_tls) {
            SSL* ssl;
            if (establish_connection(&ssl, ssl_ctx, new_socket)) {
                connections[con_index].ssl = ssl;
                std::thread(&Server::handle_tls_client, this, ssl, new_socket, std::move(address)).detach();
            } else {
                std::cout << "Failed to establish tls connection with client: " <<
                    (unsigned char)(address->sin_addr.s_addr) << std::endl;
                connections[con_index].fd = -1;
            }
        } else
            std::thread(&Server::handle_client, this, new_socket, std::move(address)).detach();
    }
}

void Server::handle_client(int socket_fd, std::unique_ptr<sockaddr_in> address) {
    std::unique_ptr<http::response> res = std::make_unique<http::response>();
    bool keep_alive = false;

    std::string ip = ip_to_str(address->sin_addr.s_addr);

    std::cout << '[' << get_time()
              << "] Client connected: "
              << ip
              << std::endl;

    try {
    keep:
        res.reset(new http::response(http::not_found()));
        std::string input = read_to_end(socket_fd);
        http::request req = http::parse_request(input);

        std::cout << '[' << get_time()
                  << "] Client: "
                  << ip
                  << " sent "
                  << req.method
                  << " request"
                  << std::endl;

        std::string uri = "";
        for (auto& r: req.uri.route)
            uri += "/" + r ;

        if (uri.empty())
            uri = "/";

        if (static_files.find(uri) != static_files.end()) {
            res.reset(new http::response(http::ok(get_content_type(uri), static_files[uri].data())));
        } else if (uri == "/" && static_files.find("/index.html") != static_files.end()) {
            res.reset(new http::response(http::ok("text/html", static_files["/index.html"].data())));
        } else if (!req.uri.route.empty()) {
            for (auto& c: controllers) {
                if (c->get_route() == req.uri.route[0]) {
                    res.reset(new http::response(c->handle(req)));
                }
            }
        }

        if (req.headers.find("Connection") != req.headers.end() &&
            req.headers["Connection"] == "keep-alive") {
            keep_alive = true;
            res->headers["Connection"] = "keep-alive";
        } else {
        	keep_alive = false;
        	res->headers["Connection"] = "close";
        }

        std::string res_str = http::serialize_response(*res);
        write(socket_fd, res_str.data(), res_str.size());

        if (keep_alive) goto keep;
    } catch (...) {
        std::cout << '[' << get_time()
              << "] Error with client: "
              << ip
              << std::endl;
    }

    for (connection& c: connections) {
        if (c.fd == socket_fd) {
            c.fd = -1;
            break;
        }
    }
    close(socket_fd);

    std::cout << '[' << get_time()
              << "] Client disconnected: "
              << ip
              << std::endl;
}

void Server::handle_tls_client(SSL* ssl, int socket_fd, std::unique_ptr<sockaddr_in> address) {
    std::unique_ptr<http::response> res = std::make_unique<http::response>(http::not_found());
    bool keep_alive = false;

    std::string ip = ip_to_str(address->sin_addr.s_addr);

    std::cout << '[' << get_time()
              << "] Client connected: "
              << ip
              << std::endl;

    try {
    keep:
        res.reset(new http::response(http::not_found()));
        std::string input = read_to_end(ssl, socket_fd);
        http::request req = http::parse_request(input);

        std::cout << '[' << get_time()
                  << "] Client: "
                  << ip
                  << " sent "
                  << req.method
                  << " request"
                  << std::endl;

        std::string uri = "";
        for (auto& r: req.uri.route)
            uri += "/" + r ;

        if (uri.empty())
            uri = "/";

        if (static_files.find(uri) != static_files.end()) {
            res.reset(new http::response(http::ok(get_content_type(uri), static_files[uri].data())));
        } else if (uri == "/" && static_files.find("/index.html") != static_files.end()) {
            res.reset(new http::response(http::ok("text/html", static_files["/index.html"].data())));
        } else if (!req.uri.route.empty()) {
            for (auto& c: controllers) {
                if (c->get_route() == req.uri.route[0]) {
                    res.reset(new http::response(c->handle(req)));
                }
            }
        }

        if (req.headers.find("Connection") != req.headers.end() &&
            req.headers["Connection"] == "keep-alive") {
            keep_alive = true;
            res->headers["Connection"] = "keep-alive";
        } else {
            keep_alive = false;
            res->headers["Connection"] = "close";
        }

        std::string res_str = http::serialize_response(*res);
        SSL_write(ssl, res_str.data(), res_str.size());

        if (keep_alive) goto keep;
    } catch (...) {
        std::cout << '[' << get_time()
              << "] Error with client: "
              << ip
              << std::endl;
    }

    for (connection& c: connections) {
        if (c.fd == socket_fd) {
            SSL_shutdown(c.ssl);
            SSL_free(c.ssl);
            c.fd = -1;
            break;
        }
    }
    close(socket_fd);

    std::cout << '[' << get_time()
              << "] Client disconnected: "
              << ip
              << std::endl;
}

void Server::listen_for_clients(int max) {
    if (listen(fd, max))
        throw std::runtime_error("Can not listen for incoming connections");
    running = true;
    connections.resize(max);
    connection* base_ptr = connections.data();
    for (connection* i = base_ptr; i < base_ptr + max; i++) {
        i->fd = -1;
    }
    start_server_loop();
}

void Server::use_static_files(const std::string &dir) {
    static_files = get_all_files(dir);
}

void Server::use_controllers(const std::vector<Controller*>& controllers) {
    this->controllers = controllers;
}

void Server::use_https(const std::string &cert_file, const std::string &key_file) {
    use_tls = true;
    init_openssl();
    ssl_ctx = create_context();
    configure_context(ssl_ctx, cert_file, key_file);
}

void Server::terminate() {
    running = false;
    close(fd);
    if (use_tls) {
        for (connection &c: connections) {
            if (c.fd != -1) {
                if (c.ssl != nullptr) {
                    SSL_shutdown(c.ssl);
                    SSL_free(c.ssl);
                }
                close(c.fd);
            }
        }
        SSL_CTX_free(ssl_ctx);
        cleanup_openssl();
    }
    else
        for (connection &c: connections) close(c.fd);
}
