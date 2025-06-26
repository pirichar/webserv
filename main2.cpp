#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <fstream>
#include <sstream>
#include <algorithm> // for std::find
#include <sys/stat.h> // for mkdir

// -- Structures --
struct ServerConfig {
    int port;
    std::string server_name;
    std::string root_directory;
};

// -- Global Variables --
bool g_running = true;

// -- Function Prototypes --
void signalHandler(int signum);
std::vector<ServerConfig> parseConfig(const char* filename);
void handleClient(int client_fd, const std::vector<ServerConfig>& configs);

// -- Signal Handler --
void signalHandler(int signum) {
    std::cout << "\nCaught signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
}

// -- Configuration Parser --
std::vector<ServerConfig> parseConfig(const char* filename) {
    std::vector<ServerConfig> configs;
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config file '" << filename << "'." << std::endl;
        exit(1);
    }

    std::string line;
    ServerConfig currentConfig;
    bool in_server_block = false;

    while (std::getline(configFile, line)) {
        std::istringstream iss(line);
        std::string key;
        std::string token;
        iss >> token;

        if (token == "server") {
            std::string brace;
            if (iss >> brace && brace == "{") {
                in_server_block = true;
                currentConfig = ServerConfig(); // Reset for new server block
                currentConfig.port = 80; // Default port
            }
        } else if (token == "}" && in_server_block) {
            configs.push_back(currentConfig);
            in_server_block = false;
        } else if (in_server_block) {
            std::string value;
            iss >> value;
            if (!value.empty() && value[value.length() - 1] == ';') {
                value.erase(value.length() - 1);
            }

            if (token == "listen") {
                currentConfig.port = atoi(value.c_str());
            } else if (token == "server_name") {
                currentConfig.server_name = value;
            } else if (token == "root") {
                currentConfig.root_directory = value;
            }
        }
    }
    configFile.close();
    return configs;
}

// -- Client Handler --
void handleClient(int client_fd, const std::vector<ServerConfig>& configs) {
    char buffer[4096] = {0};
    int valread = read(client_fd, buffer, 4096);
    if (valread <= 0) return;

    std::string request(buffer);
    std::istringstream request_stream(request);
    std::string method, path, http_version;
    request_stream >> method >> path >> http_version;

    // Find Host header to select the correct virtual server
    std::string host_header;
    size_t host_pos = request.find("Host: ");
    if (host_pos != std::string::npos) {
        size_t end_pos = request.find("\r\n", host_pos);
        host_header = request.substr(host_pos + 6, end_pos - (host_pos + 6));
    }

    // Select config based on host header
    const ServerConfig* config = &configs[0]; // Default to the first server
    for (size_t i = 0; i < configs.size(); ++i) {
        if (configs[i].server_name == host_header) {
            config = &configs[i];
            break;
        }
    }

    std::string response;
    std::string full_path = config->root_directory + path;

    if (method == "GET") {
        if (path == "/") full_path += "index.html";
        std::ifstream file(full_path.c_str());
        if (file.good()) {
            std::stringstream content_stream;
            content_stream << file.rdbuf();
            std::string body = content_stream.str();
            std::stringstream ss;
            ss << body.length();
            response = "HTTP/1.1 200 OK\r\nContent-Length: " + ss.str() + "\r\n\r\n" + body;
        } else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        }
    } else if (method == "POST") {
        std::string upload_path = config->root_directory + "/uploads";
        mkdir(upload_path.c_str(), 0755); // Ensure uploads directory exists
        std::string file_path = upload_path + path;

        size_t body_start = request.find("\r\n\r\n") + 4;
        std::string body = request.substr(body_start);

        std::ofstream outfile(file_path.c_str());
        outfile << body;
        outfile.close();
        response = "HTTP/1.1 201 Created\r\nContent-Length: 12\r\n\r\nFile Created";
    } else if (method == "DELETE") {
        if (remove(full_path.c_str()) == 0) {
            response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nFile Deleted";
        } else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        }
    } else {
        response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 18\r\n\r\n501 Not Implemented";
    }

    send(client_fd, response.c_str(), response.length(), 0);
}

// -- Main Server Function --
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::vector<ServerConfig> configs = parseConfig(argv[1]);
    if (configs.empty()) {
        std::cerr << "Error: No server configurations found." << std::endl;
        return 1;
    }

    std::map<int, int> port_to_fd;
    std::vector<pollfd> fds;

    for (size_t i = 0; i < configs.size(); ++i) {
        int port = configs[i].port;
        if (port_to_fd.find(port) == port_to_fd.end()) {
            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd < 0) {
                std::cerr << "Error: Cannot create socket for port " << port << std::endl;
                continue;
            }

            sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(port);

            if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                std::cerr << "Error: Cannot bind to port " << port << std::endl;
                close(server_fd);
                continue;
            }

            if (listen(server_fd, 10) < 0) {
                std::cerr << "Error: Cannot listen on port " << port << std::endl;
                close(server_fd);
                continue;
            }

            std::cout << "Server listening on port " << port << "..." << std::endl;
            fds.push_back((pollfd){server_fd, POLLIN, 0});
            port_to_fd[port] = server_fd;
        }
    }

    while (g_running) {
        int ret = poll(&fds[0], fds.size(), 1000); // 1s timeout to check g_running
        if (ret < 0) {
            if (!g_running) break;
            std::cerr << "Error: poll() failed" << std::endl;
            break;
        }
        if (ret == 0) continue; // Timeout

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                bool is_server = false;
                 for (std::map<int, int>::iterator it = port_to_fd.begin(); it != port_to_fd.end(); ++it) {
                    if (it->second == fds[i].fd) {
                        is_server = true;
                        break;
                    }
                }

                if (is_server) {
                    int client_fd = accept(fds[i].fd, NULL, NULL);
                    if (client_fd >= 0) {
                        std::cout << "Accepted new connection on fd " << client_fd << std::endl;
                        fds.push_back((pollfd){client_fd, POLLIN, 0});
                    }
                } else {
                    handleClient(fds[i].fd, configs);
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
            }
        }
    }

    std::cout << "Closing all connections..." << std::endl;
    for (size_t i = 0; i < fds.size(); ++i) {
        close(fds[i].fd);
    }

    return 0;
}
