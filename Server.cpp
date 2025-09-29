#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <csignal>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>

static bool g_server_running = true;

void Server::signalHandler(int signum) {
    (void)signum;
    g_server_running = false;
}

Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _sockfd(-1) {
    std::cout << "IRC Server starting on port " << _port << std::endl;
    signal(SIGINT, Server::signalHandler);
    signal(SIGQUIT, Server::signalHandler);
}

Server::~Server() {
    for (size_t i = 0; i < _fds.size(); ++i) {
        close(_fds[i].fd);
    }
}

void Server::setup() {
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0) {
        throw std::runtime_error("Failed to set socket to non-blocking");
    }

    int opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(_port);

    if (bind(_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(_sockfd, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }

    struct pollfd pfd;
    pfd.fd = _sockfd;
    pfd.events = POLLIN;
    _fds.push_back(pfd);
}

void Server::run() {
    setup();
    std::cout << "IRC Server running on port " << _port << std::endl;
    
    while (g_server_running) {
        int poll_count = poll(_fds.data(), _fds.size(), -1);
        if (poll_count < 0) {
            if (!g_server_running) break;
            throw std::runtime_error("Poll failed");
        }

        if (_fds[0].revents & POLLIN) {
            handleNewConnection();
        }

        for (size_t i = 1; i < _fds.size(); ++i) {
            if (_fds[i].revents & POLLIN) {
                handleClientData(_fds[i].fd);
            }
        }
    }
    std::cout << "\nShutting down IRC server." << std::endl;
}

void Server::handleNewConnection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(_sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Failed to accept new connection" << std::endl;
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Failed to set client socket to non-blocking" << std::endl;
        close(client_fd);
        return;
    }

    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    _fds.push_back(pfd);

    _clients.insert(std::make_pair(client_fd, ClientInfo(client_fd)));
    std::cout << "New client connected: fd " << client_fd << std::endl;
}

void Server::handleClientData(int fd) {
    char buffer[512];
    int nbytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (nbytes <= 0) {
        if (nbytes == 0) {
            std::cout << "Client fd " << fd << " disconnected" << std::endl;
        } else {
            std::cerr << "Error receiving data from client " << fd << std::endl;
        }
        removeClient(fd);
        return;
    }

    buffer[nbytes] = '\0';
    ClientInfo& client = _clients[fd];
    client.buffer += buffer;

    // Process complete messages (ending with \r\n or \n)
    size_t pos;
    while ((pos = client.buffer.find("\n")) != std::string::npos) {
        std::string message = client.buffer.substr(0, pos);
        client.buffer.erase(0, pos + 1);
        
        // Remove \r if present before \n
        if (!message.empty() && message[message.length() - 1] == '\r') {
            message = message.substr(0, message.length() - 1);
        }
        
        if (!message.empty()) {
            std::cout << "Received message: [" << message << "]" << std::endl;
            processMessage(fd, message);
        }
    }
}

void Server::removeClient(int fd) {
    ClientInfo& client = _clients[fd];
    
    // Remove from all channels
    for (std::set<std::string>::iterator it = client.channels.begin(); 
         it != client.channels.end(); ++it) {
        _channels[*it].erase(fd);
        if (_channels[*it].empty()) {
            _channels.erase(*it);
        }
    }
    
    close(fd);
    _clients.erase(fd);
    
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (_fds[i].fd == fd) {
            _fds.erase(_fds.begin() + i);
            break;
        }
    }
}

void Server::processMessage(int fd, const std::string& message) {
    std::vector<std::string> tokens = parseMessage(message);
    if (tokens.empty()) return;

    std::string command = tokens[0];
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    std::vector<std::string> params(tokens.begin() + 1, tokens.end());

    std::cout << "Client " << fd << ": " << command;
    for (size_t i = 0; i < params.size(); ++i) {
        std::cout << " [" << params[i] << "]";
    }
    std::cout << std::endl;

    // Handle IRC commands that irssi and other clients use
    if (command == "PASS") {
        handlePass(fd, params);
    } else if (command == "NICK") {
        handleNick(fd, params);
    } else if (command == "USER") {
        handleUser(fd, params);
    } else if (command == "JOIN") {
        handleJoin(fd, params);
    } else if (command == "PRIVMSG") {
        handlePrivMsg(fd, params);
    } else if (command == "QUIT") {
        handleQuit(fd, params);
    } else if (command == "PING") {
        handlePing(fd, params);
    } else if (command == "PART") {
        handlePart(fd, params);
    } else if (command == "MODE") {
        handleMode(fd, params);
    } else if (command == "WHO") {
        handleWho(fd, params);
    } else if (command == "LIST") {
        handleList(fd, params);
    } else if (command == "TOPIC") {
        // Handle TOPIC command
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, "451 :You have not registered\\r\\n");
            return;
        }
        if (params.empty()) {
            sendReply(fd, "461 TOPIC :Not enough parameters\\r\\n");
            return;
        }
        std::string channel = params[0];
        if (params.size() > 1) {
            // Set topic (just acknowledge for now)
            sendReply(fd, "332 " + client.nickname + " " + channel + " :" + params[1] + "\\r\\n");
        } else {
            // Get topic (no topic set)
            sendReply(fd, "331 " + client.nickname + " " + channel + " :No topic is set\\r\\n");
        }
    } else if (command == "NAMES") {
        // Handle NAMES command  
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, "451 :You have not registered\\r\\n");
            return;
        }
        if (!params.empty()) {
            std::string channel = params[0];
            if (_channels.find(channel) != _channels.end()) {
                std::string names = "";
                for (std::set<int>::iterator it = _channels[channel].begin(); 
                     it != _channels[channel].end(); ++it) {
                    if (!names.empty()) names += " ";
                    names += _clients[*it].nickname;
                }
                sendReply(fd, "353 " + client.nickname + " = " + channel + " :" + names + "\\r\\n");
            }
            sendReply(fd, "366 " + client.nickname + " " + channel + " :End of NAMES list\\r\\n");
        }
    } else if (command == "NOTICE") {
        // Handle NOTICE command (similar to PRIVMSG but no auto-replies)
        handlePrivMsg(fd, params); // For simplicity, treat like PRIVMSG
    } else {
        sendReply(fd, "421 " + command + " :Unknown command\\r\\n");
    }
}

std::vector<std::string> Server::parseMessage(const std::string& message) {
    std::vector<std::string> tokens;
    
    if (message.empty()) return tokens;
    
    // Find the start of the message (skip any leading whitespace)
    size_t pos = 0;
    while (pos < message.length() && (message[pos] == ' ' || message[pos] == '\t')) {
        pos++;
    }
    
    // Parse the message according to IRC RFC 2812
    // Format: [':' prefix SPACE] command [SPACE params] [SPACE ':' trailing]
    
    // Skip prefix if present (starts with ':')
    if (pos < message.length() && message[pos] == ':') {
        // Skip to next space (end of prefix)
        while (pos < message.length() && message[pos] != ' ') {
            pos++;
        }
        // Skip spaces after prefix
        while (pos < message.length() && message[pos] == ' ') {
            pos++;
        }
    }
    
    // Parse command and parameters
    while (pos < message.length()) {
        if (message[pos] == ' ') {
            // Skip multiple spaces
            while (pos < message.length() && message[pos] == ' ') {
                pos++;
            }
            continue;
        }
        
        if (message[pos] == ':') {
            // Trailing parameter - everything after ':' until end of line
            pos++; // Skip the ':'
            std::string trailing = message.substr(pos);
            // Remove trailing \r\n if present
            size_t end = trailing.find_last_not_of("\r\n");
            if (end != std::string::npos) {
                trailing = trailing.substr(0, end + 1);
            }
            if (!trailing.empty()) {
                tokens.push_back(trailing);
            }
            break;
        } else {
            // Regular parameter - until next space or ':'
            size_t start = pos;
            while (pos < message.length() && message[pos] != ' ' && message[pos] != ':') {
                pos++;
            }
            std::string param = message.substr(start, pos - start);
            // Remove trailing \r\n if it's the last parameter
            size_t end = param.find_last_not_of("\r\n");
            if (end != std::string::npos) {
                param = param.substr(0, end + 1);
            }
            if (!param.empty()) {
                tokens.push_back(param);
            }
        }
    }
    
    return tokens;
}

std::string Server::extractCommand(const std::string& message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;
    
    // Convert to uppercase for case-insensitive comparison
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    return command;
}

bool Server::isValidNickname(const std::string& nick) {
    if (nick.empty() || nick.length() > 9) return false;
    
    // First character must be letter
    if (!std::isalpha(nick[0])) return false;
    
    // Rest can be letters, digits, or special characters
    for (size_t i = 1; i < nick.length(); ++i) {
        char c = nick[i];
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '[' && c != ']' && 
            c != '{' && c != '}' && c != '\\' && c != '|') {
            return false;
        }
    }
    return true;
}

void Server::sendReply(int fd, const std::string& reply) {
    send(fd, reply.c_str(), reply.length(), 0);
}

void Server::broadcastToChannel(const std::string& channel, const std::string& message, int exclude_fd) {
    std::map<std::string, std::set<int> >::iterator it = _channels.find(channel);
    if (it != _channels.end()) {
        for (std::set<int>::iterator client_it = it->second.begin(); 
             client_it != it->second.end(); ++client_it) {
            if (*client_it != exclude_fd) {
                sendReply(*client_it, message);
            }
        }
    }
}

void Server::handlePass(int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendReply(fd, "461 PASS :Not enough parameters\r\n");
        return;
    }
    
    ClientInfo& client = _clients[fd];
    if (params[0] == _password) {
        client.authenticated = true;
        std::cout << "Client " << fd << " authenticated" << std::endl;
    } else {
        sendReply(fd, "464 :Password incorrect\r\n");
    }
}

void Server::handleNick(int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendReply(fd, "431 :No nickname given\r\n");
        return;
    }

    ClientInfo& client = _clients[fd];
    std::string new_nick = params[0];
    
    if (!isValidNickname(new_nick)) {
        sendReply(fd, "432 " + new_nick + " :Erroneous nickname\r\n");
        return;
    }
    
    // Check if nick is already in use
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
         it != _clients.end(); ++it) {
        if (it->first != fd && it->second.nickname == new_nick) {
            sendReply(fd, "433 " + new_nick + " :Nickname is already in use\r\n");
            return;
        }
    }
    
    client.nickname = new_nick;
    std::cout << "Client " << fd << " set nickname to " << new_nick << std::endl;
    
    // Check if we can complete registration
    if (client.authenticated && !client.nickname.empty() && !client.username.empty() && !client.registered) {
        client.registered = true;
        sendReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname + "\r\n");
        sendReply(fd, "002 " + client.nickname + " :Your host is ircserver, running version 1.0\r\n");
        sendReply(fd, "003 " + client.nickname + " :This server was created at startup\r\n");
        sendReply(fd, "004 " + client.nickname + " ircserver 1.0 o o\r\n");
        sendReply(fd, "375 " + client.nickname + " :- ircserver Message of the day - \r\n");
        sendReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!\r\n");
        sendReply(fd, "376 " + client.nickname + " :End of MOTD command\r\n");
        std::cout << "Client " << fd << " (" << client.nickname << ") registered" << std::endl;
    }
}

void Server::handleUser(int fd, const std::vector<std::string>& params) {
    if (params.size() < 4) {
        sendReply(fd, "461 USER :Not enough parameters\r\n");
        return;
    }

    ClientInfo& client = _clients[fd];
    client.username = params[0];
    client.hostname = params[1];
    client.realname = params[3];
    
    std::cout << "Client " << fd << " set user info: " << client.username << std::endl;
    
    // Check if we can complete registration
    if (client.authenticated && !client.nickname.empty() && !client.username.empty() && !client.registered) {
        client.registered = true;
        sendReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname + "\r\n");
        sendReply(fd, "002 " + client.nickname + " :Your host is ircserver, running version 1.0\r\n");
        sendReply(fd, "003 " + client.nickname + " :This server was created at startup\r\n");
        sendReply(fd, "004 " + client.nickname + " ircserver 1.0 o o\r\n");
        sendReply(fd, "375 " + client.nickname + " :- ircserver Message of the day - \r\n");
        sendReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!\r\n");
        sendReply(fd, "376 " + client.nickname + " :End of MOTD command\r\n");
        std::cout << "Client " << fd << " (" << client.nickname << ") registered" << std::endl;
    }
}

void Server::handleJoin(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, "461 JOIN :Not enough parameters\r\n");
        return;
    }

    std::string channel = params[0];
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    client.channels.insert(channel);
    _channels[channel].insert(fd);
    
    sendReply(fd, ":" + client.nickname + " JOIN :" + channel + "\r\n");
    sendReply(fd, "353 " + client.nickname + " = " + channel + " :" + client.nickname + "\r\n");
    sendReply(fd, "366 " + client.nickname + " " + channel + " :End of NAMES list\r\n");
    
    std::cout << "Client " << client.nickname << " joined " << channel << std::endl;
}

void Server::handlePrivMsg(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.size() < 2) {
        sendReply(fd, "461 PRIVMSG :Not enough parameters\r\n");
        return;
    }

    std::string target = params[0];
    std::string message = params[1];
    
    if (target[0] == '#') {
        // Channel message
        if (_channels.find(target) != _channels.end()) {
            std::string msg = ":" + client.nickname + " PRIVMSG " + target + " :" + message + "\r\n";
            broadcastToChannel(target, msg, fd);
            std::cout << "Channel " << target << " <" << client.nickname << "> " << message << std::endl;
        } else {
            sendReply(fd, "403 " + target + " :No such channel\r\n");
        }
    } else {
        // Private message to user
        for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
             it != _clients.end(); ++it) {
            if (it->second.nickname == target) {
                std::string msg = ":" + client.nickname + " PRIVMSG " + target + " :" + message + "\r\n";
                sendReply(it->first, msg);
                std::cout << "Private <" << client.nickname << " -> " << target << "> " << message << std::endl;
                return;
            }
        }
        sendReply(fd, "401 " + target + " :No such nick\r\n");
    }
}

void Server::handleQuit(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    std::string quit_msg = "Client Quit";
    if (!params.empty()) {
        quit_msg = params[0];
    }
    
    std::cout << "Client " << client.nickname << " quit: " << quit_msg << std::endl;
    removeClient(fd);
}

void Server::handlePing(int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendReply(fd, "409 :No origin specified\r\n");
        return;
    }
    
    // Respond with PONG
    sendReply(fd, "PONG ircserver :" + params[0] + "\r\n");
}

void Server::handlePart(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, "461 PART :Not enough parameters\r\n");
        return;
    }

    std::string channel = params[0];
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    if (client.channels.find(channel) == client.channels.end()) {
        sendReply(fd, "442 " + channel + " :You're not on that channel\r\n");
        return;
    }
    
    std::string part_msg = "Leaving";
    if (params.size() > 1) {
        part_msg = params[1];
    }
    
    client.channels.erase(channel);
    _channels[channel].erase(fd);
    
    std::string msg = ":" + client.nickname + " PART " + channel + " :" + part_msg + "\r\n";
    broadcastToChannel(channel, msg, -1); // Send to all including the one leaving
    
    if (_channels[channel].empty()) {
        _channels.erase(channel);
    }
    
    std::cout << "Client " << client.nickname << " left " << channel << std::endl;
}

void Server::handleMode(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, "461 MODE :Not enough parameters\r\n");
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#') {
        // Channel mode - just send back basic channel modes
        sendReply(fd, "324 " + client.nickname + " " + target + " +nt\r\n");
    } else {
        // User mode - just acknowledge
        sendReply(fd, "221 " + client.nickname + " +i\r\n");
    }
}

void Server::handleWho(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, "315 " + client.nickname + " * :End of WHO list\r\n");
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#') {
        // Channel WHO
        std::map<std::string, std::set<int> >::iterator it = _channels.find(target);
        if (it != _channels.end()) {
            for (std::set<int>::iterator client_it = it->second.begin(); 
                 client_it != it->second.end(); ++client_it) {
                ClientInfo& target_client = _clients[*client_it];
                sendReply(fd, "352 " + client.nickname + " " + target + " " + 
                         target_client.username + " " + target_client.hostname + 
                         " ircserver " + target_client.nickname + " H :0 " + 
                         target_client.realname + "\r\n");
            }
        }
        sendReply(fd, "315 " + client.nickname + " " + target + " :End of WHO list\r\n");
    } else {
        sendReply(fd, "315 " + client.nickname + " " + target + " :End of WHO list\r\n");
    }
}

void Server::handleList(int fd, const std::vector<std::string>& params) {
    (void)params; // Unused parameter
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    sendReply(fd, "321 " + client.nickname + " Channel :Users  Name\r\n");
    
    for (std::map<std::string, std::set<int> >::iterator it = _channels.begin(); 
         it != _channels.end(); ++it) {
        std::ostringstream oss;
        oss << it->second.size();
        sendReply(fd, "322 " + client.nickname + " " + it->first + " " + 
                 oss.str() + " :No topic\r\n");
    }
    
    sendReply(fd, "323 " + client.nickname + " :End of LIST\r\n");
}