#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace http {

class URI {
public:
    std::vector<std::string> route;
    std::map<std::string, std::string> parameters;

    URI();
    URI(const std::string& uriString);
};

struct request {
    std::string method;
    URI uri;
    std::string version = "HTTP/1.1";
    std::map<std::string, std::string> headers;
    std::string body;
};

struct response {
    std::string version = "HTTP/1.1";
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;
};

request parse_request(const std::string& input);

std::string serialize_response(response& res);

response ok();
response ok(const nlohmann::json& body);
response ok(const std::string& content_type, const std::string& body);

response created();
response created(const nlohmann::json& body);
response created(const std::string& content_type, const std::string& body);

response accepted();
response accepted(const nlohmann::json& body);
response accepted(const std::string& content_type, const std::string& body);

response no_content();

response moved_permanently(const std::string& location);
response found(const std::string& location);
response not_modified();
response temporary_redirect(const std::string& location);
response permanent_redirect(const std::string& location);

response bad_request();
response bad_request(const nlohmann::json& body);
response bad_request(const std::string& content_type, const std::string& body);

response unauthorized();
response unauthorized(const nlohmann::json& body);
response unauthorized(const std::string& content_type, const std::string& body);

response forbidden();
response forbidden(const nlohmann::json& body);
response forbidden(const std::string& content_type, const std::string& body);

response not_found();
response not_found(const nlohmann::json& body);
response not_found(const std::string& content_type, const std::string& body);

response method_not_allowed();
response method_not_allowed(const nlohmann::json& body);
response method_not_allowed(const std::string& content_type, const std::string& body);

response conflict();
response conflict(const nlohmann::json& body);
response conflict(const std::string& content_type, const std::string& body);

response unprocessable_entity();
response unprocessable_entity(const nlohmann::json& body);
response unprocessable_entity(const std::string& content_type, const std::string& body);

response too_many_requests();
response too_many_requests(const nlohmann::json& body);
response too_many_requests(const std::string& content_type, const std::string& body);

response internal_server_error();
response internal_server_error(const nlohmann::json& body);
response internal_server_error(const std::string& content_type, const std::string& body);

response not_implemented();
response not_implemented(const nlohmann::json& body);
response not_implemented(const std::string& content_type, const std::string& body);

response bad_gateway();
response bad_gateway(const nlohmann::json& body);
response bad_gateway(const std::string& content_type, const std::string& body);

response service_unavailable();
response service_unavailable(const nlohmann::json& body);
response service_unavailable(const std::string& content_type, const std::string& body);

response gateway_timeout();
response gateway_timeout(const nlohmann::json& body);
response gateway_timeout(const std::string& content_type, const std::string& body);

}

#endif // HTTP_HPP
