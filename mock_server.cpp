// mock_server.cpp
// Minimal mock IRC-like server for testing Client & Channel classes
// C++98 compatible, no fancy features, only raw sockets & poll()
// Adhil: You can extend this by replacing the "echo" part with your Client/Channel logic.

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>

// ------------------------------
// Struct to represent a mock client connection
// (Later you will replace this with your Client class)
// ------------------------------
struct MockClient {
    int fd;                     // socket file descriptor
    std::string inBuffer;       // buffer for incoming data
    std::string outBuffer;      // buffer for outgoing data
    bool connected;             // track if still connected

    MockClient(int socketFd) : fd(socketFd), inBuffer(""), outBuffer(""), connected(true) {}
};

// ------------------------------
// Global containers
// ------------------------------
std::map<int, MockClient*> clients;  // Map fd -> MockClient*
int listen_fd;                       // Listening socket

// ------------------------------
// Helper: create listening socket
// ------------------------------
int create_server_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        exit(1);
    }

    if (listen(sock, 10) < 0) {
        perror("listen");
        close(sock);
        exit(1);
    }

    return sock;
}

// ------------------------------
// Handle new client connection
// ------------------------------
void handle_new_connection() {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int new_fd = accept(listen_fd, (struct sockaddr*)&cli_addr, &cli_len);
    if (new_fd < 0) {
        perror("accept");
        return;
    }

    // Add to client map
    MockClient* client = new MockClient(new_fd);
    clients[new_fd] = client;

    std::cout << "New client connected, fd=" << new_fd << std::endl;

    // Mock welcome message
    client->outBuffer = "Welcome to Mock IRC Server!\r\n";
}

// ------------------------------
// Handle client read
// ------------------------------
void handle_client_read(int fd) {
    char buffer[512];
    std::memset(buffer, 0, sizeof(buffer));
    int n = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0) {
        // client disconnected
        std::cout << "Client disconnected, fd=" << fd << std::endl;
        close(fd);
        delete clients[fd];
        clients.erase(fd);
        return;
    }

    MockClient* client = clients[fd];
    client->inBuffer.append(buffer, n);

    // Check if a full line was received
    size_t pos;
    while ((pos = client->inBuffer.find("\n")) != std::string::npos) {
        std::string line = client->inBuffer.substr(0, pos + 1);
        client->inBuffer.erase(0, pos + 1);

        // For now: just echo back the line with a prefix
        std::string response = "[Echo] " + line;
        client->outBuffer += response;

        // You can replace the above line with:
        //   parse_command(line, client);
        //   and let your Client/Channel logic respond
    }
}

// ------------------------------
// Handle client write
// ------------------------------
void handle_client_write(int fd) {
    MockClient* client = clients[fd];
    if (client->outBuffer.empty())
        return;

    int n = send(fd, client->outBuffer.c_str(), client->outBuffer.size(), 0);
    if (n > 0) {
        client->outBuffer.erase(0, n); // remove sent bytes
    }
}

// ------------------------------
// Main event loop (poll based)
// ------------------------------
void run_server(int port) {
    listen_fd = create_server_socket(port);

    std::cout << "Mock server listening on port " << port << std::endl;

    std::vector<struct pollfd> pollfds;

    while (true) {
        // Build poll list
        pollfds.clear();

        // Add listening socket
        struct pollfd pfd;
        pfd.fd = listen_fd;
        pfd.events = POLLIN;
        pollfds.push_back(pfd);

        // Add client sockets
        for (std::map<int, MockClient*>::iterator it = clients.begin(); it != clients.end(); ++it) {
            struct pollfd cpfd;
            cpfd.fd = it->first;
            cpfd.events = POLLIN;
            if (!it->second->outBuffer.empty())
                cpfd.events |= POLLOUT;
            pollfds.push_back(cpfd);
        }

        // Wait for events
        int ret = poll(&pollfds[0], pollfds.size(), 1000);
        if (ret < 0) {
            perror("poll");
            break;
        }

        // Handle events
        for (size_t i = 0; i < pollfds.size(); i++) {
            if (pollfds[i].revents & POLLIN) {
                if (pollfds[i].fd == listen_fd) {
                    handle_new_connection();
                } else {
                    handle_client_read(pollfds[i].fd);
                }
            }
            if (pollfds[i].revents & POLLOUT) {
                handle_client_write(pollfds[i].fd);
            }
        }
    }
}

// ------------------------------
// Entry point
// ------------------------------
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./mock_server <port>" << std::endl;
        return 1;
    }
    int port = std::atoi(argv[1]);
    run_server(port);
    return 0;
}