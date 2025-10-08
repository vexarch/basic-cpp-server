#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <csignal>
#include "server.h"
#include "controller.h"
#include "http.hpp"
#include "vx_database.hpp"

const std::string HOST = "127.0.0.1";
const uint16_t PORT = 8080;

class ProductsController: public Controller {
private:
    struct Product {
        int id;
        std::string name;
        float price;
    };

    TypedTable<Product> table;

    http::response Get(http::request &req) override {
        if (req.uri.route.size() != 1)
            return http::not_found();
        nlohmann::json products = {};
        int i = 0;
        for (auto& e: table.get_all()) {
            products[i] = e.name;
            i++;
        }
        return http::ok(products);
    }
public:
    ProductsController(): Controller("products"), table("products", Schema("|id:INT32|name:STRING|price:FLOAT32|")) {
        table.clear();
        table.add_element({1, "Intel Core i9-13900K", 589.99});
        table.add_element({2, "AMD Ryzen 9 7950X", 699.99});
        table.add_element({3, "NVIDIA GeForce RTX 4090", 1599.99});
        table.add_element({4, "AMD Radeon RX 7900 XTX", 999.99});
        table.add_element({5, "Corsair Vengeance DDR5 32GB", 159.99});
        table.add_element({6, "G.Skill Trident Z5 RGB 32GB", 169.99});
    }
    ~ProductsController() {
        std::cout << "Hello" << std::endl;
        table.clear();
        fs::remove("products_table.db");
        fs::remove("products_table_strings.db");
    }
};

Server* s = nullptr;

// use the signal handler if you wanted the destractors of controllers to be called when terminating
void signal_handler(int sig) {
    std::cout << std::endl;
    if (s != nullptr) s->terminate();
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    Server server(HOST, PORT);
    server.use_static_files("example/wwwroot");
    server.use_controllers<ProductsController>();
    std::cout << "Server started on http://" << HOST << ":" << PORT << " ..." << std::endl;
    s = &server;
    server.listen_for_clients(10);

    return 0;
}
