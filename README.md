# Basic C++ HTTP Server

A minimal, lightweight HTTP server written in C++. Designed for learning how web servers work at a low level and for building simple web applications.

## Features

- Scalable endpoints via the `Controller` class
- Rich built-in library for customizable HTTP responses (status, headers, body)
- HTTPS support (OpenSSL)
- Static file serving with automatic MIME type detection
- Built-in simple database classes to make tables

## Requirements

- POSIX-compatible system (Linux, Unix, WSL) 64-bit
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

    http::response Get(http::request &req) override {
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
    Server server(HOST, PORT);
    server.use_controllers<UsersController>();
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
    <h1 align="center">Hello from C++ server</h1>
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

### Database And Tables

The project offers a built-in class to make tables with automatic loading / unloading and efficient cache store, to use it include the header:

```cpp
#include "vx_database.hpp"
```

Make a schema for the table:

```cpp
// Method 1
Schema s;
s.add_column("id", DataType::INT32);
s.add_column("name", DataType::CHAR, 16); // the last argument is used for arrays
s.add_column("description", DataType::STRING);

// Method 2
Schema s("|id:INT32|name:CHAR[32]|description:STRING");
```

Make a struct that matches the schema:

```cpp
struct user {
    int id;
    char name[16];
    std::string description;
};
```

Create a table:

```cpp
TypedTable<user> t("users", s); // s -> the schema from earlier

t.add_element({1, "admin", "add - remove - baned access"});
t.add_element({2, "user", "read - comment access"});
```

Navigate the table date:
```cpp
auto u = t.get_element(0); // returns the user on index 0

u = t.find_first([](user s) -> bool {return s.id == 1}); // return the first user with id 1

u = t.pop_all([](user s) -> bool {return true})[0]; // removes all the elements from the table and return it as a vector<user>

t.remove([](user s) -> bool {return true}, 10); // removes the first 10 elements where the condition is true

t.clear(); // reinitialize the table erasing all its data
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
- The distractors of controllers are not called when exiting
- Tables are static and lack the support for dynamic adding and removing columns
- Tables template based functions (add_element<T>, find<T>, ...) have so much overhead because of the continuos packing and unpacking of the data since it store it as one packed buffer without padding
- No built-in sorting function for tables
- Project is not fully tested, expect possible bugs

## License

This project is licensed under the MIT License. See [LICENSE](./LICENSE) for details.

This project uses OpenSSL for HTTPS support. OpenSSL is licensed under the Apache License 2.0. If you distribute binaries linked against OpenSSL, please ensure you comply with its license terms. See the [OpenSSL License](https://www.openssl.org/source/license.html) for details.
