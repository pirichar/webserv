#include "WebServer.hpp"
#include <iostream>
#include <csignal>

bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\nCaught signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char **argv) {
    /**
     * @brief Main entry point of the webserv program.
     * Parses command-line arguments, sets up signal handlers, and starts the WebServer.
     * @param argc The number of command-line arguments.
     * @param argv An array of command-line argument strings.
     * @return 0 on successful execution, 1 on error.
     */
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        WebServer server(argv[1]);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}