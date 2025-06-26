#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : _port(80), _client_max_body_size(1024 * 1024) {}

ServerConfig::~ServerConfig() {}

void ServerConfig::setPort(int port) { _port = port; }
int ServerConfig::getPort() const { return _port; }

void ServerConfig::addServerName(const std::string& name) { _server_names.push_back(name); }
const std::vector<std::string>& ServerConfig::getServerNames() const { return _server_names; }

void ServerConfig::setErrorPage(int error_code, const std::string& path) { _error_pages[error_code] = path; }
const std::string* ServerConfig::getErrorPage(int error_code) const {
    std::map<int, std::string>::const_iterator it = _error_pages.find(error_code);
    if (it != _error_pages.end()) {
        return &(it->second);
    }
    return NULL;
}

void ServerConfig::setClientMaxBodySize(size_t size) { _client_max_body_size = size; }
size_t ServerConfig::getClientMaxBodySize() const { return _client_max_body_size; }

void ServerConfig::addLocation(const Location& location) { _locations.push_back(location); }
const std::vector<Location>& ServerConfig::getLocations() const { return _locations; }
/**
 * @brief Gets the list of Location blocks for the server configuration.
 * @return A const reference to the vector of Location objects.
 */
