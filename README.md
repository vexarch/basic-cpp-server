# Basic C++ HTTP Server

A minimal, lightweight HTTP server written in C++. Designed for learning how web servers work at a low level and for building simple web applications.

## Features

- Scalable endpoints via the `Controller` class
- Rich built-in library for customizable HTTP responses (status, headers, body)
- HTTPS support (OpenSSL)
- Static file serving with automatic MIME type detection

## Requirements

- POSIX-compatible system (Linux, Unix, WSL)
- C++ compiler
- Make
- OpenSSL >= 3
- nlohmann-json (for JSON parsing/serialization)

## Installation

### Install dependencies

```bash
sudo apt update
sudo apt install build-essential libssl-dev nlohmann-json3-dev
```

### Clone the repository

```bash
git clone https://github.com/vexarch/basic-cpp-server.git
cd basic-cpp-server
```

## Usage

### Build and run the example

```bash
make example
./basic-server
```

### Build your own server

This project uses OOP principles and offers classes/namespaces for building servers without editing core source code. You can modify the [example](example/main) or create your own `Makefile` and [...] 

#### Include required headers

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
// Project headers
#include "server.h"
#include "controller.h"
#include "http.hpp"
```

#### Create Controllers

Use the `Controller` class for different endpoints:

```cpp
class UsersController : public Controller {
private:
    std::vector<std::string> users = { "Vexarch", "Someone" };

    http::response Get(http::request &req) const override {
        if (req.uri.parameters.find("name") == req.uri.parameters.end())
            return http::bad_request("text/plain", "Missing parameter");

        std::string name = req.uri.parameters["name"];
        for (const std::string& user : users) {
            if (user == name)
                return http::accepted("text/plain", "You are allowed");
        }
        return http::unauthorized("text/plain", "You are not allowed");
    }
public:
    UsersController() : Controller("users") {}
};
```

#### Start the server

```cpp
int main(int argc, char** argv) {
    controllers = { new UsersController };

    Server server(HOST, PORT);
    server.use_controllers(controllers);
    s = &server;

    std::cout << "Server started on http://" << HOST << ":" << PORT << " ..." << std::endl;
    server.listen_for_clients(10); // Max simultaneous clients

    return 0;
}
```

#### Test the server

Use curl or a browser:

```bash
curl http://127.0.0.1:1234/users?name=Vexarch
```

### HTTPS Support

To use HTTPS, create a self-signed certificate:

```bash
openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout key.pem -out cert.pem
```

Update your server code to use HTTPS:

```cpp
server.use_https("cert.pem", "key.pem"); // PEM files must be in the executable's directory
```

### Static File Serving

Create a folder (e.g., `public`) with your website files. The main page should be `index.html` located at the top level.

Sample `index.html`:

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

Serve static files:

```cpp
server.use_static_files("public"); // Replace with your folder name
```

Test:

```bash
curl http://127.0.0.1:1234/
```

## Contributing

Contributions are welcome! Please fork the repo and submit a pull request:

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes
4. Push to your branch
5. Open a PR

## Known Issues

- Server may return OK instead of NOT FOUND for missing static files
- Project is not fully tested, expect possible bugs

## License

This project is licensed under the MIT License. See [LICENSE](./LICENSE) for details.

This project uses OpenSSL for HTTPS support. OpenSSL is licensed under the Apache License 2.0. If you distribute binaries linked against OpenSSL, please ensure you comply with its license terms. See the [OpenSSL License](https://www.openssl.org/source/license.html) for details.
