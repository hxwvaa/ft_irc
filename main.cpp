#include "Server.hpp"

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    // Validate port number
    char* endptr;
    errno = 0;
    long port_long = std::strtol(argv[1], &endptr, 10);
    
    if (errno != 0 || *endptr != '\0' || endptr == argv[1]) {
        std::cerr << "Error: Invalid port number" << std::endl;
        return 1;
    }
    
    if (port_long < 1 || port_long > 65535) {
        std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
        return 1;
    }
    
    int port = static_cast<int>(port_long);
    std::string password = argv[2];
    
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        return 1;
    }

    try {
        Server server(port, password);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
