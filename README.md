# Basic HTTP server in C++

A lightweight and small HTTP server written in C++. Ideal for learning how web servers works in low level

## Features

- Endpoints support using the Controller class which make it scalable
- Rich library of built-in functions for responses that covers wide range of status with customizable headers and body
- HTTPS support via OpenSSL
- Static files support with automatic MIME type detection

## Usage

### Prerequisites

- POSIX compatible system (Linuxm, Unix, WSL)
- C++ compiler
- Make
- OpenSSL >= 3
- nlohmann **(For json parsing / serializing)**

### Installation
#### Install dependencies
```bash
sudo apt update
sudo apt install build-essential libssl-dev nlohmann-json3-dev
```

#### Clone the repository
```bash
git clone https://github.com/vexarch/basic-cpp-server.git
cd basic-cpp-server
```

### Build and run the example
```bash
make
./Basic_Server
```

### Make your own server
This project uses OOP, it offers some classes and namespaces for building a server without needing to edit the source code<br>
You can edit the [example](example/main) file or make your own [Makefile](Makefile) and main.cpp files

#### Include necessairy headers
```C++
#include <iostream>
#include <string>
#include <vector>
#include <csignal> // For proper shutdown
#include <nlohmann/json.hpp>
// The project headers
#include "server.h"
#include "controller.h"
#include "http.hpp"
```

#### Setup the clean up
This step is optional since when exiting the system regain the resources anyway but recommended for safely closing all server connection
```C++
// Global variables
Server* s;
std::vector<Controller*> controllers;

const char HOST[] = "127.0.0.1";
const int PORT = 1234;

void signal_handler(int sig) {
    if (s != nullptr) {
        s->terminate();
    }
}
```

#### Make controllers
Use the controller class to make controllers for diffrent endpoints
```C++
class UsersController: public Controller {
private:
    std::vector<std::string> users = { "Vexarch", "Someone" };

    // The syntax is the same for other functions (Post, Put, Delete, Patch and Options)
    http::response Get(http::request &req) const override {
        if (req.uri.parameters.find("name") != req.uri.parameters.end()) // The server automaticly extract parameters from URI
            return http::bad_request("text/plain", "Missing parameter");

        std::string name = req.uri.parameters["name"];

        for (std::string& user: users) {
            if (user == name)
                return http::accepted("text/plain", "You are allowed");
        }

        return http::unauthorized("text/plain", "You are not allowed");
    }
public:
    // It is important to set the route name of the controller in the constructor
    UsersController(): Controller("users") {}
}
```

#### Start the server
```C++
int main(int argc, char** argv) {
    controllers = {new UsersController};

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server server(HOST, PORT);

    server.use_controllers(controllers);
    s = &server;

    cout << "Server started on http://" << HOST << ":" << PORT << " ..." << endl;
    server.listen_for_clients(10); // The number specifies the maximum number of clients at the same time

    return 0;
}
```

#### Test the server
Use terminal (or browser) to test the server
```bash
curl http://127.0.0.1:1234/users?name=vexarch
```

#### Using HTTPS
To use HTTPS you should create a self signed certificate and key using OpenSSL
```bash
openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout key.pem -out cert.pem
```
Then use the generated files in the code
```C++
int main(int argc, char** argv) {
    controllers = {new UsersController};

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server server(HOST, PORT);

    server.use_controllers(controllers);
    s = &server;

    server.use_https("cert.pem", "key.pem"); // PEM files must be in the same directory as the executable

    cout << "Server started on https://" << HOST << ":" << PORT << " ..." << endl;
    server.listen_for_clients(10);

    return 0;
}
```

#### Using static files
Create a folder and add your website files in it, the main page name should be **index.html** and it should be the top directory of the folder since the server automaticly return index.html if the route was / (the root) .
```html
<!DOCTYPE html>
<html>
    <head>
        <title>My Website</title>
    </head>
    <body>
        <h1>Hello from C++ server</h1>
    </body>
</html>
```
Then use it in the script
```C++
int main(int argc, char** argv) {
    controllers = {new UsersController};

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server server(HOST, PORT);

    server.use_controllers(controllers);
    s = &server;

    server.use_static_files("dir_name"); // Replace the dir_name with the accual path to the website files folder

    cout << "Server started on http://" << HOST << ":" << PORT << " ..." << endl;
    server.listen_for_clients(10);

    return 0;
}
```
Use a browser or just the curl command to see the file
```bash
curl http://127.0.0.1:1234/
```

## Contributing
Contributions are welcome! Please fork the repo and submit a pull request.
- Fork the repository
- Create a feature branch (git checkout -b feature/my-feature)
- Commit your changes
- Push to your branch
- Open a PR

## Known issues
- The code does not contain any logs beside some errors logs
- Sometimes the server returns OK instead of NOT FOUND if it didn't find the static file
- The project is not tested properly so more bugs and problems exists

## License
This project is licensed under the MIT License. See [LICENSE](./LICENSE) for details.
