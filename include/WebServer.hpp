#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <vector>
#include <map>
#include <poll.h>
#include "ServerConfig.hpp" 
#include "HttpResponse.hpp" // Added this line
#include "HttpRequest.hpp" // Added this line

class WebServer {
public:
    WebServer(const std::string& config_file);
    ~WebServer();

    void run();

private:
    std::vector<ServerConfig> _configs;
    std::vector<pollfd> _fds;
    std::map<int, int> _listening_sockets; // port -> fd

    void _setup_listening_sockets();
    void _handle_new_connection(int listener_fd);
    void _handle_client_data(int client_fd);
    const ServerConfig* _get_server_config(int port, const std::string& host) const;
    const Location* _get_location(const ServerConfig* config, const std::string& uri) const;
    void _serve_static_file(const std::string& file_path, HttpResponse& response) const;
    void _generate_autoindex(const std::string& directory_path, const std::string& uri_path, HttpResponse& response) const;
    void _handle_post_request(const HttpRequest& request, const ServerConfig* server_config, const Location* location, HttpResponse& response) const;
    void _handle_delete_request(const HttpRequest& request, const Location* location, HttpResponse& response) const;
    void _execute_cgi(const HttpRequest& request, const Location* location, HttpResponse& response) const;
    void _serve_error_page(int status_code, const ServerConfig* config, HttpResponse& response) const;
    static std::string _get_status_message_static(int code);
};

#endif
