#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include "ServerConfig.hpp"

class ConfigParser {
public:
    ConfigParser(const std::string& filename);
    ~ConfigParser();

    std::vector<ServerConfig> parse();

private:
    std::string _filename;
    std::string _content;
    size_t _pos;

    void _eat_whitespace();
    std::string _next_token();
    void _parse_server_block(std::vector<ServerConfig>& configs);
    void _parse_location_block(Location& location);
};

#endif
