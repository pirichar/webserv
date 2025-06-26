#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void setStatusCode(int code);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);

    std::string toString() const;

private:
    int _status_code;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

#endif
