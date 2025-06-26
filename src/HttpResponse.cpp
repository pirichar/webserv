#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : _status_code(200) {}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatusCode(int code) { _status_code = code; }
void HttpResponse::setHeader(const std::string& name, const std::string& value) { _headers[name] = value; }
void HttpResponse::setBody(const std::string& body) { _body = body; }

std::string HttpResponse::toString() const {
    /**
     * @brief Converts the HttpResponse object into a raw HTTP response string.
     * Includes status line, headers, and body (handling chunked transfer encoding if specified).
     * @return The complete raw HTTP response string.
     */
    std::stringstream ss;
    ss << "HTTP/1.1 " << _status_code << " ";
    switch (_status_code) {
        case 200: ss << "OK"; break;
        case 201: ss << "Created"; break;
        case 204: ss << "No Content"; break;
        case 400: ss << "Bad Request"; break;
        case 403: ss << "Forbidden"; break;
        case 404: ss << "Not Found"; break;
        case 405: ss << "Method Not Allowed"; break;
        case 413: ss << "Payload Too Large"; break;
        case 500: ss << "Internal Server Error"; break;
        case 501: ss << "Not Implemented"; break;
        default: ss << ""; break;
    }
    ss << "\r\n";


    // Add Content-Length if not already set and body exists
    if (_headers.find("Content-Length") == _headers.end() && !_body.empty() && _headers.find("Transfer-Encoding") == _headers.end()) {
        ss << "Content-Length: " << _body.length() << "\r\n";
    }

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        ss << it->first << ": " << it->second << "\r\n";
    }
    ss << "\r\n"; // End of headers

    if (_headers.find("Transfer-Encoding") != _headers.end() && _headers.at("Transfer-Encoding") == "chunked") {
        // Send body in chunks
        size_t chunk_size = 1024; // Example chunk size
        for (size_t i = 0; i < _body.length(); i += chunk_size) {
            size_t current_chunk_size = std::min(chunk_size, _body.length() - i);
            ss << std::hex << current_chunk_size << "\r\n";
            ss << _body.substr(i, current_chunk_size) << "\r\n";
        }
        ss << "0\r\n\r\n"; // End of chunks
    } else {
        ss << _body;
    }
    return ss.str();
}


