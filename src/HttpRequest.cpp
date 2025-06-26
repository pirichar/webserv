#include "HttpRequest.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm> // for std::transform

HttpRequest::HttpRequest() {}

HttpRequest::~HttpRequest() {}

void HttpRequest::parse(const std::string& raw_request) {
    std::istringstream iss(raw_request);
    std::string line;

    // Parse request line
    if (std::getline(iss, line) && !line.empty()) {
        _parse_request_line(line);
    } else {
        throw std::runtime_error("Malformed request: missing request line");
    }

    // Parse headers
    while (std::getline(iss, line) && line != "\r") {
        if (line.empty() || line == "\r") break; // End of headers
        _parse_header_line(line);
    }

    // Parse body (if any)
    std::string transfer_encoding = getHeader("transfer-encoding");
    std::string content_length_str = getHeader("content-length");

    if (transfer_encoding == "chunked") {
        // Handle chunked body
        std::string chunk_size_str;
        while (std::getline(iss, chunk_size_str) && !chunk_size_str.empty()) {
            std::stringstream ss(chunk_size_str);
            size_t chunk_size;
            ss >> std::hex >> chunk_size;
            if (chunk_size == 0) {
                std::getline(iss, line); // Read the final CRLF
                break;
            }
            char* chunk_buffer = new char[chunk_size + 1];
            iss.read(chunk_buffer, chunk_size);
            chunk_buffer[chunk_size] = '\0';
            _body += chunk_buffer;
            delete[] chunk_buffer;
            std::getline(iss, line); // Read the CRLF after the chunk data
        }
    } else if (!content_length_str.empty()) {
        // Handle non-chunked body with Content-Length
        size_t content_length = static_cast<size_t>(std::atol(content_length_str.c_str()));
        if (content_length > 0) {
            std::string body_buffer(content_length, '\0');
            iss.read(&body_buffer[0], content_length);
            _body = body_buffer;
        }
    } else {
        // No Content-Length or Transfer-Encoding, body is empty
        _body = "";
    }
}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getUri() const { return _uri; }
const std::string& HttpRequest::getHttpVersion() const { return _http_version; }

const std::string& HttpRequest::getHeader(const std::string& name) const {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::map<std::string, std::string>::const_iterator it = _headers.find(lower_name);
    if (it != _headers.end()) {
        return it->second;
    }
    static const std::string empty_string = "";
    return empty_string;
}

const std::string& HttpRequest::getBody() const { return _body; }

void HttpRequest::_parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    iss >> _method >> _uri >> _http_version;
    if (_method.empty() || _uri.empty() || _http_version.empty()) {
        throw std::runtime_error("Malformed request line");
    }
}

void HttpRequest::_parse_header_line(const std::string& line) {
    /**
     * @brief Parses a single HTTP header line.
     * Extracts header name and value, and stores them in a map.
     * Header names are converted to lowercase for case-insensitive lookup.
     * @param line The header line string.
     * @throws std::runtime_error if the header line is malformed.
     */
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Malformed header line: " + line);
    }
    std::string name = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    // Trim whitespace from value
    size_t first_char = value.find_first_not_of(" \t\r\n");
    size_t last_char = value.find_last_not_of(" \t\r\n");
    if (std::string::npos == first_char) {
        value = "";
    } else {
        value = value.substr(first_char, (last_char - first_char + 1));
    }

    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    _headers[name] = value;
}
