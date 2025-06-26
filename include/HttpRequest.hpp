#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
public:
    HttpRequest();
    ~HttpRequest();

    void parse(const std::string& raw_request);

    const std::string& getMethod() const; // GET, POST, DELETE
    const std::string& getUri() const;
    const std::string& getHttpVersion() const;
    const std::string& getHeader(const std::string& name) const;
    const std::string& getBody() const;

private:
    std::string _method;
    std::string _uri;
    std::string _http_version;
    std::map<std::string, std::string> _headers;
    std::string _body;

    void _parse_request_line(const std::string& line);
    void _parse_header_line(const std::string& line);
};

#endif
