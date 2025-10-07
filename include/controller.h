#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "http.hpp"
#include <string>

class Controller {
protected:
    std::string route;
    virtual http::response Get(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
    virtual http::response Post(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
    virtual http::response Put(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
    virtual http::response Patch(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
    virtual http::response Delete(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
    virtual http::response Options(http::request& req) {
        (void)req; // Used to silence the unused parameter warning at compile time
        return http::not_implemented();
    }
public:
    Controller(const std::string& route): route(route) {}
    virtual ~Controller() = default;
    std::string get_route() const {
        return route;
    }

    // Basic handling for the http request
    virtual http::response handle(http::request& req) {
        if (req.method == "GET") {
            return Get(req);
        } else if (req.method == "POST") {
            return Post(req);
        } else if (req.method == "PUT") {
            return Put(req);
        } else if (req.method == "PATCH") {
            return Patch(req);
        } else if (req.method == "DELETE") {
            return Delete(req);
        } else if (req.method == "OPTIONS") {
            return Options(req);
        } else {
            return http::bad_request();
        }
    }
};

#endif // CONTROLLER_H
