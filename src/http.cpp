#include "http.hpp"

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

using namespace std;
using namespace http;

namespace http {

URI::URI() {}

URI::URI(const string& uriString) {
    string pathPart, queryPart;

    size_t queryPos = uriString.find('?');
    if (queryPos != string::npos) {
        pathPart = uriString.substr(0, queryPos);
        queryPart = uriString.substr(queryPos + 1);
    } else {
        pathPart = uriString;
    }

    if (!pathPart.empty() && pathPart != "/") {
        if (pathPart.front() == '/') {
            pathPart = pathPart.substr(1);
        }

        if (!pathPart.empty() && pathPart.back() == '/') {
            pathPart.pop_back();
        }

        istringstream pathStream(pathPart);
        string segment;
        while (getline(pathStream, segment, '/')) {
            if (!segment.empty()) {
                route.push_back(segment);
            }
        }
    }

    if (!queryPart.empty()) {
        istringstream queryStream(queryPart);
        string param;
        while (getline(queryStream, param, '&')) {
            size_t equalPos = param.find('=');
            if (equalPos != string::npos) {
                string key = param.substr(0, equalPos);
                string value = param.substr(equalPos + 1);
                parameters[key] = value;
            } else {
                parameters[param] = "";
            }
        }
    }
}

request parseRequest(const string& input) {
    request req;
    istringstream stream(input);
    string line;

    if (getline(stream, line)) {
        istringstream lineStream(line);
        string uri;
        lineStream >> req.method >> uri >> req.version;
        req.uri = URI(uri);
    }

    while (getline(stream, line) && line != "\r") {
        if (line.empty()) break;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t colonPos = line.find(':');
        if (colonPos != string::npos) {
            string key = line.substr(0, colonPos);
            string value = line.substr(colonPos + 2);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            req.headers[key] = value;
        }
    }

    ostringstream bodyStream;
    bodyStream << stream.rdbuf();
    req.body = bodyStream.str();

    return req;
}

string serializeResponse(response& res) {
    ostringstream stream;
    bool has_body = !res.body.empty();

    stream << res.version << " " << res.status_code << " " << res.status_message << "\r\n";

    if (has_body) {
        res.headers["Content-length"] = to_string(res.body.size());
    }

    for (const auto& header : res.headers) {
        stream << header.first << ": " << header.second << "\r\n";
    }

    if (has_body) {
        stream << "\r\n";
        stream << res.body;
    }

    return stream.str();
}

response ok() {
    response res;
    res.status_code = 200;
    res.status_message = "OK";
    return res;
}

response ok(const nlohmann::json& body) {
    response res;
    res.status_code = 200;
    res.status_message = "OK";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response ok(const string& content_type, const string& body) {
    response res;
    res.status_code = 200;
    res.status_message = "OK";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response created() {
    response res;
    res.status_code = 201;
    res.status_message = "Created";
    return res;
}

response created(const nlohmann::json& body) {
    response res;
    res.status_code = 201;
    res.status_message = "Created";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response created(const string& content_type, const string& body) {
    response res;
    res.status_code = 201;
    res.status_message = "Created";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response accepted() {
    response res;
    res.status_code = 202;
    res.status_message = "Accepted";
    return res;
}

response accepted(const nlohmann::json& body) {
    response res;
    res.status_code = 202;
    res.status_message = "Accepted";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response accepted(const string& content_type, const string& body) {
    response res;
    res.status_code = 202;
    res.status_message = "Accepted";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response no_content() {
    response res;
    res.status_code = 204;
    res.status_message = "No Content";
    return res;
}

response moved_permanently(const string& location) {
    response res;
    res.status_code = 301;
    res.status_message = "Moved Permanently";
    res.headers["Location"] = location;
    return res;
}

response found(const string& location) {
    response res;
    res.status_code = 302;
    res.status_message = "Found";
    res.headers["Location"] = location;
    return res;
}

response not_modified() {
    response res;
    res.status_code = 304;
    res.status_message = "Not Modified";
    return res;
}

response temporary_redirect(const string& location) {
    response res;
    res.status_code = 307;
    res.status_message = "Temporary Redirect";
    res.headers["Location"] = location;
    return res;
}

response permanent_redirect(const string& location) {
    response res;
    res.status_code = 308;
    res.status_message = "Permanent Redirect";
    res.headers["Location"] = location;
    return res;
}

response bad_request() {
    response res;
    res.status_code = 400;
    res.status_message = "Bad Request";
    return res;
}

response bad_request(const nlohmann::json& body) {
    response res;
    res.status_code = 400;
    res.status_message = "Bad Request";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response bad_request(const string& content_type, const string& body) {
    response res;
    res.status_code = 400;
    res.status_message = "Bad Request";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response unauthorized() {
    response res;
    res.status_code = 401;
    res.status_message = "Unauthorized";
    return res;
}

response unauthorized(const nlohmann::json& body) {
    response res;
    res.status_code = 401;
    res.status_message = "Unauthorized";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response unauthorized(const string& content_type, const string& body) {
    response res;
    res.status_code = 401;
    res.status_message = "Unauthorized";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response forbidden() {
    response res;
    res.status_code = 403;
    res.status_message = "Forbidden";
    return res;
}

response forbidden(const nlohmann::json& body) {
    response res;
    res.status_code = 403;
    res.status_message = "Forbidden";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response forbidden(const string& content_type, const string& body) {
    response res;
    res.status_code = 403;
    res.status_message = "Forbidden";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response not_found() {
    response res;
    res.status_code = 404;
    res.status_message = "Not Found";
    return res;
}

response not_found(const nlohmann::json& body) {
    response res;
    res.status_code = 404;
    res.status_message = "Not Found";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response not_found(const string& content_type, const string& body) {
    response res;
    res.status_code = 404;
    res.status_message = "Not Found";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response method_not_allowed() {
    response res;
    res.status_code = 405;
    res.status_message = "Method Not Allowed";
    return res;
}

response method_not_allowed(const nlohmann::json& body) {
    response res;
    res.status_code = 405;
    res.status_message = "Method Not Allowed";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response method_not_allowed(const string& content_type, const string& body) {
    response res;
    res.status_code = 405;
    res.status_message = "Method Not Allowed";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response conflict() {
    response res;
    res.status_code = 409;
    res.status_message = "Conflict";
    return res;
}

response conflict(const nlohmann::json& body) {
    response res;
    res.status_code = 409;
    res.status_message = "Conflict";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response conflict(const string& content_type, const string& body) {
    response res;
    res.status_code = 409;
    res.status_message = "Conflict";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response unprocessable_entity() {
    response res;
    res.status_code = 422;
    res.status_message = "Unprocessable Entity";
    return res;
}

response unprocessable_entity(const nlohmann::json& body) {
    response res;
    res.status_code = 422;
    res.status_message = "Unprocessable Entity";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response unprocessable_entity(const string& content_type, const string& body) {
    response res;
    res.status_code = 422;
    res.status_message = "Unprocessable Entity";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response too_many_requests() {
    response res;
    res.status_code = 429;
    res.status_message = "Too Many Requests";
    return res;
}

response too_many_requests(const nlohmann::json& body) {
    response res;
    res.status_code = 429;
    res.status_message = "Too Many Requests";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response too_many_requests(const string& content_type, const string& body) {
    response res;
    res.status_code = 429;
    res.status_message = "Too Many Requests";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response internal_server_error() {
    response res;
    res.status_code = 500;
    res.status_message = "Internal Server Error";
    return res;
}

response internal_server_error(const nlohmann::json& body) {
    response res;
    res.status_code = 500;
    res.status_message = "Internal Server Error";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response internal_server_error(const string& content_type, const string& body) {
    response res;
    res.status_code = 500;
    res.status_message = "Internal Server Error";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response not_implemented() {
    response res;
    res.status_code = 501;
    res.status_message = "Not Implemented";
    return res;
}

response not_implemented(const nlohmann::json& body) {
    response res;
    res.status_code = 501;
    res.status_message = "Not Implemented";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response not_implemented(const string& content_type, const string& body) {
    response res;
    res.status_code = 501;
    res.status_message = "Not Implemented";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response bad_gateway() {
    response res;
    res.status_code = 502;
    res.status_message = "Bad Gateway";
    return res;
}

response bad_gateway(const nlohmann::json& body) {
    response res;
    res.status_code = 502;
    res.status_message = "Bad Gateway";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response bad_gateway(const string& content_type, const string& body) {
    response res;
    res.status_code = 502;
    res.status_message = "Bad Gateway";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response service_unavailable() {
    response res;
    res.status_code = 503;
    res.status_message = "Service Unavailable";
    return res;
}

response service_unavailable(const nlohmann::json& body) {
    response res;
    res.status_code = 503;
    res.status_message = "Service Unavailable";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response service_unavailable(const string& content_type, const string& body) {
    response res;
    res.status_code = 503;
    res.status_message = "Service Unavailable";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response gateway_timeout() {
    response res;
    res.status_code = 504;
    res.status_message = "Gateway Timeout";
    return res;
}

response gateway_timeout(const nlohmann::json& body) {
    response res;
    res.status_code = 504;
    res.status_message = "Gateway Timeout";
    res.body = body.dump();
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

response gateway_timeout(const string& content_type, const string& body) {
    response res;
    res.status_code = 504;
    res.status_message = "Gateway Timeout";
    res.body = body;
    res.headers["Content-Type"] = content_type;
    res.headers["Content-Length"] = to_string(res.body.size());
    return res;
}

} // http
