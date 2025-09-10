#include <iostream>
#include <nlohmann/json.hpp>
#include <csignal>
#include <string>
#include <vector>
#include <list>
#include "server.h"
#include "controller.h"
#include "http.hpp"

using namespace std;

const char HOST[] = "127.0.0.1";
const int PORT = 8080;

Server* s = nullptr;
vector<Controller*> controllers;

void signal_handler(int sig) {
    if (s != nullptr) {
        s->terminate();
    }
}

class ProductsController: public Controller {
private:
    vector<string> products = {"PC", "Car", "Door", "Rat"};
    http::response Get(http::request &req) const override {
        // cout << "Request received" << endl;
        if (req.uri.route.size() != 1)
            return http::not_found();
        nlohmann::json j = products;
        return http::ok(j);
    }
public:
    ProductsController(): Controller("products") {}
    ~ProductsController() {}
};

int main()
{
    controllers = {new ProductsController};
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    Server server(HOST, PORT);
    server.use_static_files();
    server.use_controllers(controllers);
    //server.use_https("ssl/cert.pem", "ssl/key.pem");
    s = &server;
    cout << "Server started on http://" << HOST << ":" << PORT << " ..." << endl;
    server.listen_for_clients(10);

    return 0;
}
