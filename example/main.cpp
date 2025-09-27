#include <iostream>
#include <nlohmann/json.hpp>
#include <csignal>
#include <string>
#include <vector>
#include "server.h"
#include "controller.h"
#include "http.hpp"

const std::string HOST = "127.0.0.1";
const uint16_t PORT = 8080;

Server* s = nullptr;
vector<Controller*> controllers;

class ProductsController: public Controller {
private:
    vector<string> products = {"Intel Core i9-13900K", "AMD Ryzen 9 7950X", "NVIDIA GeForce RTX 4090", "AMD Radeon RX 7900 XTX", "Corsair Vengeance DDR5 32GB", "G.Skill Trident Z5 RGB 32GB", "Samsung 990 PRO 2TB SSD", "WD Black SN850X 2TB", "ASUS ROG Strix Z790-E", "MSI MPG X670E Carbon", "Corsair RM850x PSU"};
    http::response Get(http::request &req) override {
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
    Server server(HOST, PORT);
    server.use_static_files("example/wwwroot");
    server.use_controllers(controllers);
    s = &server;
    cout << "Server started on http://" << HOST << ":" << PORT << " ..." << endl;
    server.listen_for_clients(10);

    return 0;
}
