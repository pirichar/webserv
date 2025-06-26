#include "WebServer.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h> // For fcntl
#include <cstring> // For memset
#include <stdexcept>
#include <sys/stat.h> // For stat
#include <dirent.h> // For opendir, readdir
#include <fstream> // For std::ifstream, std::ofstream
#include <sstream> // For std::stringstream
#include <cerrno> // For errno
#include <sys/wait.h> // For waitpid
#include <vector> // For std::vector

// Global flag for graceful shutdown
extern bool g_running;

WebServer::WebServer(const std::string& config_file) {
    ConfigParser parser(config_file);
    _configs = parser.parse();
    _setup_listening_sockets();
}

WebServer::~WebServer() {
    for (size_t i = 0; i < _fds.size(); ++i) {
        close(_fds[i].fd);
    }
}

void WebServer::_setup_listening_sockets() {
    for (size_t i = 0; i < _configs.size(); ++i) {
        int port = _configs[i].getPort();
        if (_listening_sockets.find(port) == _listening_sockets.end()) {
            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd < 0) {
                throw std::runtime_error("Cannot create socket for port " + std::to_string(port));
            }

            // Set socket to non-blocking
            if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
                close(server_fd);
                throw std::runtime_error("Cannot set socket to non-blocking for port " + std::to_string(port));
            }

            // Allow socket to reuse address and port (for quick restart)
            int opt = 1;
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                close(server_fd);
                throw std::runtime_error("Cannot set SO_REUSEADDR for port " + std::to_string(port));
            }
#ifdef SO_REUSEPORT
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
                close(server_fd);
                throw std::runtime_error("Cannot set SO_REUSEPORT for port " + std::to_string(port));
            }
#endif

            sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(port);

            if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                close(server_fd);
                throw std::runtime_error("Cannot bind to port " + std::to_string(port));
            }

            if (listen(server_fd, 1024) < 0) { // SOMAXCONN is often 128, 1024 is a common high value
                close(server_fd);
                throw std::runtime_error("Cannot listen on port " + std::to_string(port));
            }

            std::cout << "Server listening on port " << port << "..." << std::endl;
            _fds.push_back((pollfd){server_fd, POLLIN, 0});
            _listening_sockets[port] = server_fd;
        }
    }
}

void WebServer::_handle_new_connection(int listener_fd) {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        // Handle EAGAIN/EWOULDBLOCK for non-blocking sockets
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error: accept() failed" << std::endl;
        }
    } else {
        // Set client socket to non-blocking
        if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
            close(client_fd);
            std::cerr << "Error: Cannot set client socket to non-blocking" << std::endl;
            return;
        }
        std::cout << "New connection accepted on fd " << client_fd << std::endl;
        _fds.push_back((pollfd){client_fd, POLLIN, 0});
    }
}

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

void WebServer::_serve_static_file(const std::string& file_path, HttpResponse& response) const {
    std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        response.setStatusCode(404);
        response.setBody("404 Not Found");
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    response.setBody(buffer.str());
    response.setStatusCode(200);

    // Basic Content-Type detection (can be improved)
    if (file_path.find(".html") != std::string::npos) {
        response.setHeader("Content-Type", "text/html");
    } else if (file_path.find(".css") != std::string::npos) {
        response.setHeader("Content-Type", "text/css");
    } else if (file_path.find(".js") != std::string::npos) {
        response.setHeader("Content-Type", "application/javascript");
    } else if (file_path.find(".jpg") != std::string::npos || file_path.find(".jpeg") != std::string::npos) {
        response.setHeader("Content-Type", "image/jpeg");
    } else if (file_path.find(".png") != std::string::npos) {
        response.setHeader("Content-Type", "image/png");
    } else {
        response.setHeader("Content-Type", "application/octet-stream");
    }
}

void WebServer::_generate_autoindex(const std::string& directory_path, const std::string& uri_path, HttpResponse& response) const {
    std::stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html><head><title>Index of " << uri_path << "</title></head><body>\n";
    ss << "<h1>Index of " << uri_path << "</h1><hr><pre>\n";

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(directory_path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") continue;
            ss << "<a href=\"" << uri_path << (uri_path[uri_path.length() - 1] == '/' ? "" : "/") << name << "\">" << name << "</a>\n";
        }
        closedir(dir);
    } else {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Could not open directory");
        return;
    }

    ss << "</pre><hr></body></html>\n";
    response.setStatusCode(200);
    response.setHeader("Content-Type", "text/html");
    response.setBody(ss.str());
}

void WebServer::_handle_post_request(const HttpRequest& request, const ServerConfig* server_config, const Location* location, HttpResponse& response) const {
    // Check client_max_body_size
    if (request.getBody().length() > server_config->getClientMaxBodySize()) {
        response.setStatusCode(413);
        response.setBody("413 Payload Too Large");
        return;
    }

    std::string upload_dir = location->getRoot();
    // Ensure upload directory exists
    if (mkdir(upload_dir.c_str(), 0755) == -1 && errno != EEXIST) {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Could not create upload directory");
        return;
    }

    std::string file_path = upload_dir + request.getUri().substr(location->getPath().length());
    std::ofstream outfile(file_path.c_str(), std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Could not open file for writing");
        return;
    }

    outfile << request.getBody();
    outfile.close();

    response.setStatusCode(201);
    response.setBody("201 Created");
}

void WebServer::_handle_delete_request(const HttpRequest& request, const Location* location, HttpResponse& response) const {
    std::string file_path = location->getRoot() + request.getUri();

    struct stat s;
    if (stat(file_path.c_str(), &s) != 0) {
        response.setStatusCode(404);
        response.setBody("404 Not Found");
        return;
    }

    if (!(s.st_mode & S_IFREG)) { // Not a regular file
        response.setStatusCode(403);
        response.setBody("403 Forbidden: Cannot delete a directory");
        return;
    }

    if (remove(file_path.c_str()) == 0) {
        response.setStatusCode(200);
        response.setBody("200 OK: File deleted");
    } else {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Could not delete file");
    }
}

void WebServer::_execute_cgi(const HttpRequest& request, const Location* location, HttpResponse& response) const {
    const std::string* cgi_path_ptr = location->getCgiPath(".php"); // Assuming .php for now
    if (!cgi_path_ptr) {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: CGI path not configured");
        return;
    }
    std::string cgi_path = *cgi_path_ptr;

    std::cout << "DEBUG: CGI Path: " << cgi_path << std::endl;

    int pipe_in[2]; // Parent writes to child's stdin
    int pipe_out[2]; // Child writes to parent's stdout

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Pipe creation failed");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        response.setStatusCode(500);
        response.setBody("500 Internal Server Error: Fork failed");
        return;
    } else if (pid == 0) { // Child process
        close(pipe_in[1]); // Close write end of pipe_in
        close(pipe_out[0]); // Close read end of pipe_out

        dup2(pipe_in[0], STDIN_FILENO); // Redirect stdin
        dup2(pipe_out[1], STDOUT_FILENO); // Redirect stdout

        close(pipe_in[0]);
        close(pipe_out[1]);

        // Prepare arguments for execve
        std::vector<char*> argv_vec;
        argv_vec.push_back(const_cast<char*>(cgi_path.c_str()));
        std::string script_relative_path = request.getUri().substr(location->getPath().length());
        std::string script_full_path = location->getRoot() + script_relative_path;
        argv_vec.push_back(const_cast<char*>(script_full_path.c_str()));
        argv_vec.push_back(NULL);

        std::cout << "DEBUG (Child): argv_vec: ";
        for (size_t i = 0; argv_vec[i] != NULL; ++i) {
            std::cout << argv_vec[i] << " ";
        }
        std::cout << std::endl;

        // Prepare environment variables
        std::vector<std::string> env_strings;
        env_strings.push_back("REQUEST_METHOD=" + request.getMethod());
        size_t query_pos = request.getUri().find('?');
        if (query_pos != std::string::npos) {
            env_strings.push_back("QUERY_STRING=" + request.getUri().substr(query_pos + 1));
        } else {
            env_strings.push_back("QUERY_STRING=");
        }
        
        std::stringstream ss_content_length;
        ss_content_length << request.getBody().length();
        env_strings.push_back("CONTENT_LENGTH=" + ss_content_length.str());

        env_strings.push_back("CONTENT_TYPE=" + request.getHeader("Content-Type"));
        env_strings.push_back("SCRIPT_FILENAME=" + script_full_path);
        env_strings.push_back("REDIRECT_STATUS=200"); // For PHP-CGI

        std::vector<char*> envp_vec;
        for (size_t i = 0; i < env_strings.size(); ++i) {
            envp_vec.push_back(const_cast<char*>(env_strings[i].c_str()));
        }
        envp_vec.push_back(NULL);

        std::cout << "DEBUG (Child): envp_vec: " << std::endl;
        for (size_t i = 0; envp_vec[i] != NULL; ++i) {
            std::cout << "  " << envp_vec[i] << std::endl;
        }

        execve(cgi_path.c_str(), &argv_vec[0], &envp_vec[0]);
        // If execve returns, an error occurred
        perror("execve failed"); // Print execve error
        exit(1); // Exit child process with error
    } else { // Parent process
        close(pipe_in[0]); // Close read end of pipe_in
        close(pipe_out[1]); // Close write end of pipe_out

        // Write request body to CGI stdin
        write(pipe_in[1], request.getBody().c_str(), request.getBody().length());
        close(pipe_in[1]); // Close write end to signal EOF to child

        // Read CGI stdout
        std::string cgi_output;
        char cgi_buffer[4096];
        ssize_t bytes_read_cgi;
        while ((bytes_read_cgi = read(pipe_out[0], cgi_buffer, sizeof(cgi_buffer) - 1)) > 0) {
            cgi_buffer[bytes_read_cgi] = '\0';
            cgi_output += cgi_buffer;
        }
        close(pipe_out[0]);

        int status;
        waitpid(pid, &status, 0);

        // std::cout << "DEBUG (Parent): CGI raw output:\n---\n" << cgi_output << "\n---" << std::endl;
        // std::cout << "DEBUG (Parent): Child process exited with status: " << status << std::endl;

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // Parse CGI output into HTTP response
            size_t header_end = cgi_output.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                response.setStatusCode(500);
                response.setBody("500 Internal Server Error: Malformed CGI output");
                return;
            }

            std::string cgi_headers = cgi_output.substr(0, header_end);
            std::string cgi_body = cgi_output.substr(header_end + 4);

            std::istringstream header_stream(cgi_headers);
            std::string line;
            while (std::getline(header_stream, line) && !line.empty()) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string name = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
                    // Trim whitespace
                    size_t first_char = value.find_first_not_of(" \t\r\n");
                    size_t last_char = value.find_last_not_of(" \t\r\n");
                    if (std::string::npos == first_char) {
                        value = "";
                    } else {
                        value = value.substr(first_char, (last_char - first_char + 1));
                    }
                    response.setHeader(name, value);
                }
            }
            response.setStatusCode(200); // Assuming 200 OK for successful CGI
            response.setBody(cgi_body);

        } else {
            response.setStatusCode(500);
            response.setBody("500 Internal Server Error: CGI script failed");
        }
    }
}

std::string WebServer::_get_status_message_static(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "";
    }
}

void WebServer::_serve_error_page(int status_code, const ServerConfig* config, HttpResponse& response) const {
    std::string error_body = WebServer::_get_status_message_static(status_code);
    std::string content_type = "text/plain";

    if (config) {
        const std::string* error_page_path = config->getErrorPage(status_code);
        if (error_page_path) {
            std::ifstream file(error_page_path->c_str(), std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                error_body = buffer.str();
                content_type = "text/html";
            }
        }
    }
    response.setStatusCode(status_code);
    response.setHeader("Content-Type", content_type);
    response.setBody(error_body);
}

void WebServer::_handle_client_data(int client_fd) {
    std::string raw_request_data;
    char buffer[4096];
    ssize_t bytes_read;
    
    size_t content_length = 0;
    bool headers_parsed = false;
    std::string headers_str;
    HttpResponse response;
    const ServerConfig* server_config = NULL; // Declare here for broader scope

    // Read headers first
    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        raw_request_data.append(buffer, bytes_read);

        size_t header_end_pos = raw_request_data.find("\r\n\r\n");
        if (header_end_pos != std::string::npos) {
            headers_str = raw_request_data.substr(0, header_end_pos);
            headers_parsed = true;
            break;
        }
    }

    if (bytes_read <= 0 && !headers_parsed) { // Error or client disconnected before headers
        if (bytes_read == 0) {
            std::cout << "Client disconnected on fd " << client_fd << std::endl;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error: recv() failed on fd " << client_fd << std::endl;
        }
        close(client_fd);
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].fd == client_fd) {
                _fds.erase(_fds.begin() + i);
                break;
            }
        }
        return;
    }

    // Parse headers to get Content-Length
    HttpRequest temp_request;
    try {
        temp_request.parse(headers_str + "\r\n\r\n"); // Parse only headers to get Content-Length
        std::string cl_str = temp_request.getHeader("content-length");
        if (!cl_str.empty()) {
            content_length = static_cast<size_t>(std::atol(cl_str.c_str()));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing headers: " << e.what() << std::endl;
        _serve_error_page(400, NULL, response); // Bad Request
        std::string response_str = response.toString();
        send(client_fd, response_str.c_str(), response_str.length(), 0);
        close(client_fd);
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].fd == client_fd) {
                _fds.erase(_fds.begin() + i);
                break;
            }
        }
        return;
    }

    // Read remaining body if Content-Length is present
    size_t body_start_pos = raw_request_data.find("\r\n\r\n");
    if (body_start_pos == std::string::npos) {
        // Should not happen if headers_parsed is true, but for safety
        _serve_error_page(400, NULL, response);
        std::string response_str = response.toString();
        send(client_fd, response_str.c_str(), response_str.length(), 0);
        close(client_fd);
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].fd == client_fd) {
                _fds.erase(_fds.begin() + i);
                break;
            }
        }
        return;
    }
    body_start_pos += 4; // Move past \r\n\r\n


    size_t body_already_read = raw_request_data.length() - body_start_pos;

    while (body_already_read < content_length && (bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        raw_request_data.append(buffer, bytes_read);
        body_already_read = raw_request_data.length() - body_start_pos;
    }

    std::cout << "Received data from fd " << client_fd << ":\n" << raw_request_data << std::endl;

    try {
        HttpRequest request;
        request.parse(raw_request_data);

        std::cout << "Parsed Request:" << std::endl;
        std::cout << "  Method: " << request.getMethod() << std::endl;
        std::cout << "  URI: " << request.getUri() << std::endl;
        std::cout << "  HTTP Version: " << request.getHttpVersion() << std::endl;
        std::cout << "  Host Header: " << request.getHeader("Host") << std::endl;
        std::cout << "  Body: " << request.getBody() << std::endl;

        // Get the port the client connected to
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if (getsockname(client_fd, (struct sockaddr*)&addr, &len) == -1) {
            throw std::runtime_error("getsockname failed");
        }
        int port = ntohs(addr.sin_port);

        server_config = _get_server_config(port, request.getHeader("Host"));
        if (!server_config) {
            _serve_error_page(500, NULL, response); // No server config found, use default 500
        } else {
            const Location* location = _get_location(server_config, request.getUri());
            if (!location) {
                _serve_error_page(404, server_config, response);
            } else {
                // Check allowed methods
                const std::vector<std::string>& allowed_methods = location->getAllowedMethods();
                bool method_allowed = false;
                for (size_t i = 0; i < allowed_methods.size(); ++i) {
                    if (allowed_methods[i] == request.getMethod()) {
                        method_allowed = true;
                        break;
                    }
                }
                if (!method_allowed) {
                    _serve_error_page(405, server_config, response);
                } else {
                    // Check for CGI
                    size_t dot_pos = request.getUri().find('.');
                    std::string extension;
                    if (dot_pos != std::string::npos) {
                        extension = request.getUri().substr(dot_pos);
                    }

                    if (!extension.empty() && location->getCgiPath(extension)) {
                        _execute_cgi(request, location, response);
                    } else if (request.getMethod() == "GET") {
                        std::string full_path = location->getRoot() + request.getUri();
                        struct stat s;
                        if (stat(full_path.c_str(), &s) == 0) {
                            if (s.st_mode & S_IFDIR) { // It's a directory
                                std::string index_file_path = full_path + (full_path[full_path.length() - 1] == '/' ? "" : "/") + location->getIndex();
                                std::ifstream index_file(index_file_path.c_str());
                                if (index_file.good()) {
                                    _serve_static_file(index_file_path, response);
                                } else if (location->getAutoIndex()) {
                                    _generate_autoindex(full_path, request.getUri(), response);
                                } else {
                                    _serve_error_page(403, server_config, response);
                                }
                            } else if (s.st_mode & S_IFREG) { // It's a regular file
                                _serve_static_file(full_path, response);
                            } else { // Not a regular file or directory
                                _serve_error_page(403, server_config, response);
                            }
                        } else {
                            _serve_error_page(404, server_config, response);
                        }
                    } else if (request.getMethod() == "POST") {
                        _handle_post_request(request, server_config, location, response);
                    } else if (request.getMethod() == "DELETE") {
                        _handle_delete_request(request, location, response);
                    } else {
                        _serve_error_page(501, server_config, response);
                    }
                }
            }

        }

    } catch (const std::exception& e) {
        std::cerr << "Error processing request: " << e.what() << std::endl;
        _serve_error_page(500, server_config, response); // Use server_config for 500 error page
    }

    std::string response_str = response.toString();
    send(client_fd, response_str.c_str(), response_str.length(), 0);

    // For now, close connection after sending response
    close(client_fd);
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (_fds[i].fd == client_fd) {
            _fds.erase(_fds.begin() + i);
            break;
        }
    }
}


const ServerConfig* WebServer::_get_server_config(int port, const std::string& host) const {
    const ServerConfig* default_server = NULL;
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].getPort() == port) {
            // Check for server_name match
            const std::vector<std::string>& server_names = _configs[i].getServerNames();
            for (size_t j = 0; j < server_names.size(); ++j) {
                if (server_names[j] == host) {
                    return &(_configs[i]);
                }
            }
            // If no server_name matches, the first server block for that port is the default
            if (!default_server) {
                default_server = &(_configs[i]);
            }
        }
    }
    return default_server;
}

const Location* WebServer::_get_location(const ServerConfig* config, const std::string& uri) const {
    const Location* best_match = NULL;
    size_t max_match_len = 0;

    const std::vector<Location>& locations = config->getLocations();
    for (size_t i = 0; i < locations.size(); ++i) {
        const std::string& location_path = locations[i].getPath();
        if (uri.find(location_path) == 0) { // URI starts with location path
            if (location_path.length() > max_match_len) {
                max_match_len = location_path.length();
                best_match = &(locations[i]);
            }
        }
    }
    return best_match;
}

void WebServer::run() {
    /**
     * @brief Runs the main server event loop.
     * Uses poll() to monitor listening and client sockets for incoming events.
     * Handles new connections and processes client data.
     */
    while (g_running) {
        int ret = poll(_fds.data(), _fds.size(), 1000); // 1 second timeout

        if (ret < 0) {
            if (g_running) {
                std::cerr << "Error: poll() failed" << std::endl;
            }
            break;
        }

        if (ret == 0) {
            continue; // Timeout, no events
        }

        // Iterate through fds to check for events
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].revents & POLLIN) {
                bool is_listener = false;
                for (std::map<int, int>::iterator it = _listening_sockets.begin(); it != _listening_sockets.end(); ++it) {
                    if (it->second == _fds[i].fd) {
                        is_listener = true;
                        break;
                    }
                }

                if (is_listener) {
                    _handle_new_connection(_fds[i].fd);
                } else {
                    _handle_client_data(_fds[i].fd);
                    // After handling client data, the client_fd might be closed and removed from _fds
                    // So we need to adjust the loop index
                    if (i < _fds.size() && _fds[i].fd == -1) { // Check if the current fd was removed
                        i--;
                    }
                }
            }
        }
    }
    std::cout << "Server shutting down..." << std::endl;
}