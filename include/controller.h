#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "http.hpp"
#include <string>

using namespace std;

class Controller {
protected:
    string route;
    virtual http::response Get(http::request& req) const {
        return http::not_implemented();
    }
    virtual http::response Post(http::request& req) const {
        return http::not_implemented();
    }
    virtual http::response Put(http::request& req) const {
        return http::not_implemented();
    }
    virtual http::response Patch(http::request& req) const {
        return http::not_implemented();
    }
    virtual http::response Delete(http::request& req) const {
        return http::not_implemented();
    }
    virtual http::response Options(http::request& req) const {
        return http::not_implemented();
    }
public:
    Controller(const string& route): route(route) {}
    //virtual ~Controller() = default; // this line causes compile time errors
    string get_route() const {
        return route;
    }

    // Basic handling for the http request
    http::response handle(http::request& req) const {
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
