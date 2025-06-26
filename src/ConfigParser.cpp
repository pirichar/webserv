#include "ConfigParser.hpp"
#include <fstream>
#include <stdexcept>

ConfigParser::ConfigParser(const std::string& filename) : _filename(filename), _pos(0) {
    std::ifstream file(_filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file");
    }
    _content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

ConfigParser::~ConfigParser() {}

std::vector<ServerConfig> ConfigParser::parse() {
    std::vector<ServerConfig> configs;
    while (_pos < _content.length()) {
        _eat_whitespace();
        if (_pos >= _content.length()) break;
        std::string token = _next_token();
        if (token == "server") {
            _parse_server_block(configs);
        } else {
            throw std::runtime_error("Unexpected token in config file: " + token);
        }
    }
    return configs;
}

void ConfigParser::_eat_whitespace() {
    while (_pos < _content.length() && isspace(_content[_pos])) {
        _pos++;
    }
}

std::string ConfigParser::_next_token() {
    _eat_whitespace();
    if (_pos >= _content.length()) return "";

    char c = _content[_pos];
    if (c == '{' || c == '}' || c == ';') {
        _pos++;
        return std::string(1, c);
    }

    size_t start = _pos;
    while (_pos < _content.length() && !isspace(_content[_pos]) && _content[_pos] != '{' && _content[_pos] != '}' && _content[_pos] != ';') {
        _pos++;
    }
    return _content.substr(start, _pos - start);
}

void ConfigParser::_parse_server_block(std::vector<ServerConfig>& configs) {
    ServerConfig config;
    _eat_whitespace();
    if (_next_token() != "{") {
        throw std::runtime_error("Expected '{' after server token");
    }

    while (true) {
        _eat_whitespace();
        std::string token = _next_token();
        if (token == "}") break;

        if (token == "listen") {
            config.setPort(atoi(_next_token().c_str()));
            if (_next_token() != ";") throw std::runtime_error("Expected ';' after port");
        } else if (token == "server_name") {
            while (true) {
                std::string name = _next_token();
                if (name.find(';') != std::string::npos) {
                    config.addServerName(name.substr(0, name.length() - 1));
                    break;
                }
                config.addServerName(name);
            }
        } else if (token == "error_page") {
            int code = atoi(_next_token().c_str());
            std::string path = _next_token();
            if (path.find(';') != std::string::npos) {
                 path = path.substr(0, path.length() - 1);
            }
            config.setErrorPage(code, path);
             if (_content[_pos-1] != ';') {
                if (_next_token() != ";") throw std::runtime_error("Expected ';' after error_page path");
            }
        } else if (token == "client_max_body_size") {
            std::string size_str = _next_token();
            size_t size;
            char unit = size_str[size_str.length() - 1];
            if (unit == 'm' || unit == 'k') {
                size = atoi(size_str.substr(0, size_str.length() - 1).c_str());
                if (unit == 'm') size *= (1024 * 1024);
                else if (unit == 'k') size *= 1024;
            } else {
                size = atoi(size_str.c_str());
            }
            config.setClientMaxBodySize(size);
            if (_next_token() != ";") throw std::runtime_error("Expected ';' after client_max_body_size");
        } else if (token == "location") {
            Location location;
            location.setPath(_next_token());
            _parse_location_block(location);
            config.addLocation(location);
        } else {
            throw std::runtime_error("Unknown directive in server block: " + token);
        }
    }
    configs.push_back(config);
}

void ConfigParser::_parse_location_block(Location& location) {
    /**
     * @brief Parses a 'location' block within a 'server' block.
     * Extracts directives like 'root', 'allowed_methods', 'autoindex', 'index', and 'cgi_path'.
     * @param location A reference to the Location object to populate.
     * @throws std::runtime_error if a syntax error or unknown directive is found.
     */
    _eat_whitespace();
    if (_next_token() != "{") {
        throw std::runtime_error("Expected '{' after location path");
    }

    while (true) {
        _eat_whitespace();
        std::string token = _next_token();
        if (token == "}") break;

        if (token == "root") {
            location.setRoot(_next_token());
            if (_next_token() != ";") throw std::runtime_error("Expected ';' after root");
        } else if (token == "allowed_methods") {
            while (true) {
                std::string method = _next_token();
                if (method.find(';') != std::string::npos) {
                    location.addAllowedMethod(method.substr(0, method.length() - 1));
                    break;
                }
                location.addAllowedMethod(method);
            }
        } else if (token == "autoindex") {
            location.setAutoIndex(_next_token() == "on");
            if (_next_token() != ";") throw std::runtime_error("Expected ';' after autoindex");
        } else if (token == "index") {
            location.setIndex(_next_token());
            if (_next_token() != ";") throw std::runtime_error("Expected ';' after index");
        } else if (token == "cgi_path") {
            std::string ext = _next_token();
            std::string path = _next_token();
             if (path.find(';') != std::string::npos) {
                 path = path.substr(0, path.length() - 1);
            }
            location.setCgiPath(ext, path);
            if (_content[_pos-1] != ';') {
                if (_next_token() != ";") throw std::runtime_error("Expected ';' after cgi_path");
            }
        } else {
            throw std::runtime_error("Unknown directive in location block: " + token);
        }
    }
}
