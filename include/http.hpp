#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using namespace std;

namespace http {

class URI {
public:
    vector<string> route;
    map<string, string> parameters;

    URI();
    URI(const string& uriString);
};

struct request {
    string method;
    URI uri;
    string version = "HTTP/1.1";
    map<string, string> headers;
    string body;
};

struct response {
    string version = "HTTP/1.1";
    int status_code;
    string status_message;
    map<string, string> headers;
    string body;
};

request parse_request(const string& input);

string serialize_response(response& res);

response ok();
response ok(const nlohmann::json& body);
response ok(const string& content_type, const string& body);

response created();
response created(const nlohmann::json& body);
response created(const string& content_type, const string& body);

response accepted();
response accepted(const nlohmann::json& body);
response accepted(const string& content_type, const string& body);

response no_content();

response moved_permanently(const string& location);
response found(const string& location);
response not_modified();
response temporary_redirect(const string& location);
response permanent_redirect(const string& location);

response bad_request();
response bad_request(const nlohmann::json& body);
response bad_request(const string& content_type, const string& body);

response unauthorized();
response unauthorized(const nlohmann::json& body);
response unauthorized(const string& content_type, const string& body);

response forbidden();
response forbidden(const nlohmann::json& body);
response forbidden(const string& content_type, const string& body);

response not_found();
response not_found(const nlohmann::json& body);
response not_found(const string& content_type, const string& body);

response method_not_allowed();
response method_not_allowed(const nlohmann::json& body);
response method_not_allowed(const string& content_type, const string& body);

response conflict();
response conflict(const nlohmann::json& body);
response conflict(const string& content_type, const string& body);

response unprocessable_entity();
response unprocessable_entity(const nlohmann::json& body);
response unprocessable_entity(const string& content_type, const string& body);

response too_many_requests();
response too_many_requests(const nlohmann::json& body);
response too_many_requests(const string& content_type, const string& body);

response internal_server_error();
response internal_server_error(const nlohmann::json& body);
response internal_server_error(const string& content_type, const string& body);

response not_implemented();
response not_implemented(const nlohmann::json& body);
response not_implemented(const string& content_type, const string& body);

response bad_gateway();
response bad_gateway(const nlohmann::json& body);
response bad_gateway(const string& content_type, const string& body);

response service_unavailable();
response service_unavailable(const nlohmann::json& body);
response service_unavailable(const string& content_type, const string& body);

response gateway_timeout();
response gateway_timeout(const nlohmann::json& body);
response gateway_timeout(const string& content_type, const string& body);

}

#endif // HTTP_HPP
