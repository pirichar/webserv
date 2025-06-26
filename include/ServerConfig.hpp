#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "Location.hpp"

class ServerConfig {
public:
    ServerConfig();
    ~ServerConfig();

    // Getters and Setters
    void setPort(int port);
    int getPort() const;

    void addServerName(const std::string& name);
    const std::vector<std::string>& getServerNames() const;

    void setErrorPage(int error_code, const std::string& path);
    const std::string* getErrorPage(int error_code) const;

    void setClientMaxBodySize(size_t size);
    size_t getClientMaxBodySize() const;

    void addLocation(const Location& location);
    const std::vector<Location>& getLocations() const;

private:
    int _port;
    std::vector<std::string> _server_names;
    std::map<int, std::string> _error_pages;
    size_t _client_max_body_size;
    std::vector<Location> _locations;
};

#endif
