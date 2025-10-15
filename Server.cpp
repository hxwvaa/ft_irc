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
#include <cerrno>

static bool g_server_running = true;

const std::string Server::SERVER_NAME = "A_DreamServ";

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
    pfd.revents = 0;  // Initialize revents to avoid uninitialized memory
    _fds.push_back(pfd);
}

void Server::run() {
    setup();
    // std::cout << "IRC Server running on port " << _port << std::endl;
    
    while (g_server_running) {
        int poll_count = poll(_fds.data(), _fds.size(), -1);
        if (poll_count < 0) {
            if (errno == EINTR) {
                if (!g_server_running) break;
                continue; // Interrupted by signal, continue loop
            }
            throw std::runtime_error("Poll failed");
        }

        if (_fds[0].revents & POLLIN) {
            handleNewConnection();
        }

        // Collect file descriptors to process to avoid iterator invalidation
        //Checking Which fds have data to read and collect then into fds_to_process
        std::vector<int> fds_to_process;
        for (size_t i = 1; i < _fds.size(); ++i) {
            if (_fds[i].revents & POLLIN) {
                fds_to_process.push_back(_fds[i].fd);
            }
        }
        
        // Process collected fds (safe even if removeClient is called)
        for (size_t i = 0; i < fds_to_process.size(); ++i) {
            // Check if client still exists (might have been removed)
            if (_clients.find(fds_to_process[i]) != _clients.end()) {
                handleClientData(fds_to_process[i]);
            }
        }
    }
    // std::cout << "\nShutting down IRC server." << std::endl;
}

void Server::handleNewConnection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(_sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No pending connections, not an error for non-blocking socket
            return;
        }
        std::cerr << "Failed to accept new connection: " << strerror(errno) << std::endl;
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
    pfd.revents = 0;  // Initialize revents to avoid uninitialized memory
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
            removeClient(fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error receiving data from client " << fd << ": " << strerror(errno) << std::endl;
            removeClient(fd);
        }
        // If EAGAIN/EWOULDBLOCK, just return without removing client
        return;
    }

    buffer[nbytes] = '\0';
    
    // Check if client still exists (might have been removed)
    if (_clients.find(fd) == _clients.end()) {
        return;
    }
    
    ClientInfo& client = _clients[fd];
    
    // Protect against buffer overflow
    const size_t MAX_BUFFER_SIZE = 8192; // 8KB limit
    if (client.buffer.length() + nbytes > MAX_BUFFER_SIZE) {
        std::cerr << "Buffer overflow protection: Client " << fd << " exceeded buffer limit" << std::endl;
        removeClient(fd);
        return;
    }
    
    client.buffer += buffer;

    // Process complete messages (ending with \r\n or \n)
    size_t pos;
    while ((pos = client.buffer.find("\n")) != std::string::npos) {
        // Check if client still exists before processing
        if (_clients.find(fd) == _clients.end()) {
            return;
        }
        
        std::string message = client.buffer.substr(0, pos);
        client.buffer.erase(0, pos + 1);
        
        // Remove \r if present before \n
        if (!message.empty() && message[message.length() - 1] == '\r') {
            message = message.substr(0, message.length() - 1);
        }
        
        if (!message.empty()) {
            std::cout << "Received message: [" << message << "]" << std::endl;
            processMessage(fd, message);
            
            // Check again if client still exists after processing message
            // (processMessage might call handleQuit which removes the client)
            if (_clients.find(fd) == _clients.end()) {
                return;
            }
        }
    }
}

void Server::removeClient(int fd) {
    std::map<int, ClientInfo>::iterator client_it = _clients.find(fd);
    if (client_it == _clients.end()) {
        close(fd);
        for (size_t i = 0; i < _fds.size(); ++i) {
            if (_fds[i].fd == fd) {
                _fds.erase(_fds.begin() + i);
                break;
            }
        }
        return;
    }
    
    ClientInfo& client = client_it->second;
    
    // Only send QUIT message if client was registered
    if (client.registered && !client.nickname.empty()) {
        std::string quit_msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " QUIT :Client disconnected\r\n";
        for (std::set<std::string>::iterator it = client.channels.begin(); 
             it != client.channels.end(); ++it) {
            broadcastToChannel(*it, quit_msg, fd);
            _channels[*it].members.erase(fd);
            _channels[*it].operators.erase(fd);
            if (_channels[*it].members.empty()) {
                _channels.erase(*it);
            }
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

    // std::cout << "Client " << fd << ": " << command;
    // for (size_t i = 0; i < params.size(); ++i) {
    //     std::cout << " [" << params[i] << "]";
    // }
    // std::cout << std::endl;

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
    } else if (command == "KICK") {
        handleKick(fd, params);
    } else if (command == "INVITE") {
        handleInvite(fd, params);
    } else if (command == "CAP") {
        // Handle CAP (Client Capability) command for modern IRC clients like irssi
        // We don't support any capabilities, so just acknowledge
        if (!params.empty()) {
            std::string subcmd = params[0];
            std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);
            if (subcmd == "LS") {
                // Client asking for supported capabilities - we support none
                sendReply(fd, "CAP * LS :\r\n");
            } else if (subcmd == "END") {
                // Client ending capability negotiation - just acknowledge
                // No response needed for CAP END
            } else if (subcmd == "REQ") {
                // Client requesting capabilities - we don't support any
                sendReply(fd, "CAP * NAK\r\n");
            }
        }
    } else if (command == "TOPIC") {
        // Handle TOPIC command
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
            return;
        }
        if (params.empty()) {
            sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " TOPIC :Not enough parameters"));
            return;
        }
        std::string channel = params[0];
        
        // Check if channel exists
        if (_channels.find(channel) == _channels.end()) {
            sendReply(fd, formatServerReply(fd, "403 " + client.nickname + " " + channel + " :No such channel"));
            return;
        }
        
        ChannelInfo& chanInfo = _channels[channel];
        
        // Check if user is in the channel
        if (chanInfo.members.find(fd) == chanInfo.members.end()) {
            sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
            return;
        }
        
        if (params.size() > 1) {
            // Set topic
            // Check if topic is restricted to operators (+t mode)
            if (chanInfo.topicRestricted) {
                // Only operators can set the topic when +t is enabled
                if (chanInfo.operators.find(fd) == chanInfo.operators.end()) {
                    sendReply(fd, formatServerReply(fd, "482 " + client.nickname + " " + channel + " :You're not channel operator"));
                    return;
                }
            }
            
            // Set the new topic
            std::string newTopic = params[1];
            chanInfo.topic = newTopic;
            
            // Broadcast topic change to all channel members
            std::string topicMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " TOPIC " + channel + " :" + newTopic + "\r\n";
            broadcastToChannel(channel, topicMsg, -1);
        } else {
            // Get topic
            if (chanInfo.topic.empty()) {
                sendReply(fd, formatServerReply(fd, "331 " + client.nickname + " " + channel + " :No topic is set"));
            } else {
                sendReply(fd, formatServerReply(fd, "332 " + client.nickname + " " + channel + " :" + chanInfo.topic));
            }
        }
    } else if (command == "NAMES") {
        // Handle NAMES command  
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
            return;
        }
        if (!params.empty()) {
            std::string channel = params[0];
            if (_channels.find(channel) != _channels.end()) {
                std::string names = "";
                for (std::set<int>::iterator it = _channels[channel].members.begin(); 
                     it != _channels[channel].members.end(); ++it) {
                    if (!names.empty()) names += " ";
                    // Add @ prefix for operators
                    if (_channels[channel].operators.find(*it) != _channels[channel].operators.end()) {
                        names += "@";
                    }
                    names += _clients[*it].nickname;
                }
                sendReply(fd, formatServerReply(fd, "353 " + client.nickname + " = " + channel + " :" + names));
            }
            sendReply(fd, formatServerReply(fd, "366 " + client.nickname + " " + channel + " :End of NAMES list"));
        }
    // } else if (command == "NOTICE") {
    //     // Handle NOTICE command (similar to PRIVMSG but no auto-replies)
    //     handlePrivMsg(fd, params); // For simplicity, treat like PRIVMSG
    } else if (command == "WHOIS") {
        // Handle WHOIS command - show user information
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
            return;
        }
        if (params.empty()) {
            sendReply(fd, formatServerReply(fd, "431 " + client.nickname + " :No nickname given"));
            return;
        }
        std::string target_nick = params[0];
        bool found = false;
        for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
             it != _clients.end(); ++it) {
            if (it->second.nickname == target_nick) {
                ClientInfo& target = it->second;
                sendReply(fd, formatServerReply(fd, "311 " + client.nickname + " " + target.nickname + " " + 
                         target.username + " " + target.hostname + " * :" + target.realname));
                // List channels the user is in
                if (!target.channels.empty()) {
                    std::string channels_list = "";
                    for (std::set<std::string>::iterator ch_it = target.channels.begin();
                         ch_it != target.channels.end(); ++ch_it) {
                        if (!channels_list.empty()) channels_list += " ";
                        channels_list += *ch_it;
                    }
                    sendReply(fd, formatServerReply(fd, "319 " + client.nickname + " " + target.nickname + " :" + channels_list));
                }
                sendReply(fd, formatServerReply(fd, "312 " + client.nickname + " " + target.nickname + " " + SERVER_NAME + " :IRC Server"));
                sendReply(fd, formatServerReply(fd, "318 " + client.nickname + " " + target.nickname + " :End of WHOIS list"));
                found = true;
                break;
            }
        }
        if (!found) {
            sendReply(fd, formatServerReply(fd, "401 " + client.nickname + " " + target_nick + " :No such nick/channel"));
            sendReply(fd, formatServerReply(fd, "318 " + client.nickname + " " + target_nick + " :End of WHOIS list"));
        }
    } else if (command == "USERHOST") {
        // Handle USERHOST command - return user@host for given nicknames
        ClientInfo& client = _clients[fd];
        if (!client.registered) {
            sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
            return;
        }
        std::string response = "302 " + client.nickname + " :";
        for (size_t i = 0; i < params.size(); ++i) {
            for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
                 it != _clients.end(); ++it) {
                if (it->second.nickname == params[i]) {
                    if (i > 0) response += " ";
                    response += it->second.nickname + "=+" + it->second.username + "@" + it->second.hostname;
                    break;
                }
            }
        }
        sendReply(fd, formatServerReply(fd, response));
    } else {
        ClientInfo& client = _clients[fd];
        sendReply(fd, formatServerReply(fd, "421 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " " + command + " :Unknown command"));
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
    size_t total_sent = 0;
    size_t len = reply.length();
    const char* data = reply.c_str();
    
    while (total_sent < len) {
        ssize_t sent = send(fd, data + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full, wait a bit and retry
                continue;
            }
            // Actual error - client may have disconnected
            // std::cerr << "Error sending to client " << fd << ": " << strerror(errno) << std::endl;
            return;
        }
        total_sent += sent;
    }
}

void Server::broadcastToChannel(const std::string& channel, const std::string& message, int exclude_fd) {
    std::map<std::string, ChannelInfo>::iterator it = _channels.find(channel);
    if (it != _channels.end()) {
        for (std::set<int>::iterator client_it = it->second.members.begin(); 
             client_it != it->second.members.end(); ++client_it) {
            if (*client_it != exclude_fd) {
                sendReply(*client_it, message);
            }
        }
    }
}

// Format server numeric reply with proper prefix
std::string Server::formatServerReply(int fd, const std::string& numericAndParams) const {
    std::string nick = "*";
    std::map<int, ClientInfo>::const_iterator it = _clients.find(fd);
    if (it != _clients.end() && !it->second.nickname.empty()) {
        nick = it->second.nickname;
    }
    return ":" + SERVER_NAME + " " + numericAndParams + "\r\n";
}

// Format user message with proper hostmask prefix
std::string Server::formatUserMessage(int fd, const std::string& command) const {
    std::map<int, ClientInfo>::const_iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        return ":" + it->second.nickname + "!" + it->second.username + "@" + it->second.hostname + " " + command + "\r\n";
    }
    return ":" + SERVER_NAME + " " + command + "\r\n";
}

// Helper function: Check if a client is a channel operator
bool Server::isChannelOperator(const std::string& channel, int fd) {
    std::map<std::string, ChannelInfo>::iterator it = _channels.find(channel);
    if (it == _channels.end()) return false;
    return it->second.operators.find(fd) != it->second.operators.end();
}

// Helper function: Check if a client is in a channel
bool Server::isClientInChannel(const std::string& channel, int fd) {
    std::map<std::string, ChannelInfo>::iterator it = _channels.find(channel);
    if (it == _channels.end()) return false;
    return it->second.members.find(fd) != it->second.members.end();
}

// Helper function: Get client fd by nickname
int Server::getClientFdByNick(const std::string& nickname) {
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
         it != _clients.end(); ++it) {
        if (it->second.nickname == nickname) {
            return it->first;
        }
    }
    return -1;
}

// Helper function: Convert string to int safely
bool Server::stringToInt(const std::string& str, int& result) {
    if (str.empty()) return false;
    char* end;
    long val = std::strtol(str.c_str(), &end, 10);
    if (*end != '\0') return false;
    result = static_cast<int>(val);
    return true;
}

// Helper function: Split string by delimiter
std::vector<std::string> splitByComma(const std::string& str) {
    std::vector<std::string> result;
    std::string current;
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == ',') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += str[i];
        }
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}


void Server::handlePass(int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendReply(fd, formatServerReply(fd, "461 * PASS :Not enough parameters"));
        return;
    }
    
    ClientInfo& client = _clients[fd];
    if (params[0] == _password) {
        client.authenticated = true;
        // std::cout << "Client " << fd << " authenticated" << std::endl;
    } else {
        sendReply(fd, formatServerReply(fd, "464 * :Password incorrect"));
    }
}

void Server::handleNick(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    
    // Check if client has authenticated first
    if (!client.authenticated) {
        sendReply(fd, formatServerReply(fd, "464 * :Password required"));
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, formatServerReply(fd, "431 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :No nickname given"));
        return;
    }

    std::string new_nick = params[0];
    
    // Validate nickname format
    if (!isValidNickname(new_nick)) {
        sendReply(fd, formatServerReply(fd, "432 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " " + new_nick + " :Erroneous nickname"));
        return;
    }
    
    // Check if nickname is already in use by another client
    for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
         it != _clients.end(); ++it) {
        if (it->first != fd && it->second.nickname == new_nick) {
            sendReply(fd, formatServerReply(fd, "433 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " " + new_nick + " :Nickname is already in use"));
            return;
        }
    }
    
    std::string old_nick = client.nickname;
    client.nickname = new_nick;
    
    // If user was already registered, notify channels about nick change
    if (client.registered && !old_nick.empty()) {
        std::string nick_msg = ":" + old_nick + "!" + client.username + "@" + client.hostname + " NICK :" + new_nick + "\r\n";
        for (std::set<std::string>::iterator it = client.channels.begin(); 
             it != client.channels.end(); ++it) {
            broadcastToChannel(*it, nick_msg, -1);
        }
        sendReply(fd, nick_msg);
    }
    
    // std::cout << "Client " << fd << " set nickname: " << new_nick << std::endl;
    
    // Check if we can complete registration (only if not already registered)
    if (!client.registered && client.authenticated && !client.nickname.empty() && !client.username.empty()) {
        client.registered = true;
        sendReply(fd, formatServerReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname));
        sendReply(fd, formatServerReply(fd, "002 " + client.nickname + " :Your host is " + SERVER_NAME + ", running version 1.0"));
        sendReply(fd, formatServerReply(fd, "003 " + client.nickname + " :This server was created today"));
        sendReply(fd, formatServerReply(fd, "004 " + client.nickname + " " + SERVER_NAME + " 1.0 o o"));
        sendReply(fd, formatServerReply(fd, "375 " + client.nickname + " :- " + SERVER_NAME + " Message of the day -"));
        sendReply(fd, formatServerReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!"));
        sendReply(fd, formatServerReply(fd, "376 " + client.nickname + " :End of MOTD command"));
        // std::cout << "Client " << fd << " completed registration as " << client.nickname << std::endl;
    }
}

void Server::handleUser(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    
    // Check if client has authenticated first
    if (!client.authenticated) {
        sendReply(fd, formatServerReply(fd, "464 * :Password required"));
        return;
    }
    
    // Reject USER command if already registered
    if (client.registered) {
        sendReply(fd, formatServerReply(fd, "462 " + client.nickname + " :You may not reregister"));
        return;
    }
    
    if (params.size() < 4) {
        sendReply(fd, formatServerReply(fd, "461 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " USER :Not enough parameters"));
        return;
    }

    client.username = params[0];
    // params[1] is mode (unused), params[2] is unused
    // Set hostname to localhost (in a real server, would use client's actual hostname)
    client.hostname = "localhost";
    client.realname = params[3];
    
    // std::cout << "Client " << fd << " set user info: " << client.username << std::endl;
    
    // Check if we can complete registration (only if not already registered)
    if (!client.registered && client.authenticated && !client.nickname.empty() && !client.username.empty()) {
        client.registered = true;
        sendReply(fd, formatServerReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname));
        sendReply(fd, formatServerReply(fd, "002 " + client.nickname + " :Your host is " + SERVER_NAME + ", running version 1.0"));
        sendReply(fd, formatServerReply(fd, "003 " + client.nickname + " :This server was created today"));
        sendReply(fd, formatServerReply(fd, "004 " + client.nickname + " " + SERVER_NAME + " 1.0 o o"));
        sendReply(fd, formatServerReply(fd, "375 " + client.nickname + " :- " + SERVER_NAME + " Message of the day -"));
        sendReply(fd, formatServerReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!"));
        sendReply(fd, formatServerReply(fd, "376 " + client.nickname + " :End of MOTD command"));
        // std::cout << "Client " << fd << " completed registration as " << client.nickname << std::endl;
    }
}
void Server::handleJoin(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " JOIN :Not enough parameters"));
        return;
    }

    // Parse channels and keys
    std::vector<std::string> channels = splitByComma(params[0]);
    std::vector<std::string> keys;
    
    if (params.size() > 1) {
        keys = splitByComma(params[1]);
    }
    
    // Join each channel
    for (size_t i = 0; i < channels.size(); ++i) {
        std::string channel = channels[i];
        std::string key = "";
        
        // Get corresponding key if available
        if (i < keys.size()) {
            key = keys[i];
        }
        
        // Ensure channel name starts with #
        if (channel.empty()) continue;
        if (channel[0] != '#') {
            channel = "#" + channel;
        }
        
        // Check if already in channel
        if (client.channels.find(channel) != client.channels.end()) {
            continue; // Already in this channel, skip
        }
        
        // Check if channel exists and has restrictions
        bool channelExists = _channels.find(channel) != _channels.end();
        
        if (channelExists) {
            ChannelInfo& chanInfo = _channels[channel];
            
            // Check invite-only mode
            if (chanInfo.inviteOnly) {
                // Check if user is invited
                if (chanInfo.invited.find(fd) == chanInfo.invited.end()) {
                    sendReply(fd, formatServerReply(fd, "473 " + client.nickname + " " + channel + " :Cannot join channel (+i)"));
                    continue; // Skip this channel
                }
                // User is invited, remove from invite list after successful join attempt
                chanInfo.invited.erase(fd);
            }
            
            // Check user limit
            if (chanInfo.userLimit > 0 && chanInfo.members.size() >= chanInfo.userLimit) {
                sendReply(fd, formatServerReply(fd, "471 " + client.nickname + " " + channel + " :Cannot join channel (+l)"));
                continue; // Skip this channel
            }
            
            // Check key (password)
            if (!chanInfo.key.empty()) {
                if (key != chanInfo.key) {
                    sendReply(fd, formatServerReply(fd, "475 " + client.nickname + " " + channel + " :Cannot join channel (+k)"));
                    continue; // Skip this channel
                }
            }
        }
        
        // Add user to channel
        client.channels.insert(channel);
        _channels[channel].members.insert(fd);
        
        // If first member, make them operator
        if (_channels[channel].members.size() == 1) {
            _channels[channel].operators.insert(fd);
        }
        
        // Then notify all members (including the one who just joined) about the JOIN
        std::string join_msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " JOIN :" + channel + "\r\n";
        broadcastToChannel(channel, join_msg, -1); // Send to everyone including the joiner
        
        // Build list of all users in the channel (including the one who just joined)
        std::string names = "";
        for (std::set<int>::iterator it = _channels[channel].members.begin(); 
             it != _channels[channel].members.end(); ++it) {
            if (!names.empty()) names += " ";
            // Add @ prefix for operators
            if (_channels[channel].operators.find(*it) != _channels[channel].operators.end()) {
                names += "@";
            }
            names += _clients[*it].nickname;
        }
        
        sendReply(fd, formatServerReply(fd, "353 " + client.nickname + " = " + channel + " :" + names));
        sendReply(fd, formatServerReply(fd, "366 " + client.nickname + " " + channel + " :End of NAMES list"));
        
        // std::cout << "Client " << client.nickname << " joined " << channel << std::endl;
    }
}

void Server::handlePrivMsg(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " PRIVMSG :Not enough parameters"));
        return;
    }

    std::string target = params[0];
    std::string message = params[1];
    
    if (target[0] == '#') {
        // Channel message
        if (_channels.find(target) == _channels.end()) {
            sendReply(fd, formatServerReply(fd, "403 " + client.nickname + " " + target + " :No such channel"));
            return;
        }
        
        // Check if sender is a member of the channel
        if (client.channels.find(target) == client.channels.end()) {
            sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + target + " :You're not on that channel"));
            return;
        }
        
        std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PRIVMSG " + target + " :" + message + "\r\n";
        broadcastToChannel(target, msg, fd);
        // std::cout << "Channel " << target << " <" << client.nickname << "> " << message << std::endl;
    } else {
        // Private message to user
        for (std::map<int, ClientInfo>::iterator it = _clients.begin(); 
             it != _clients.end(); ++it) {
            if (it->second.nickname == target) {
                std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PRIVMSG " + target + " :" + message + "\r\n";
                sendReply(it->first, msg);
                // std::cout << "Private <" << client.nickname << " -> " << target << "> " << message << std::endl;
                return;
            }
        }
        sendReply(fd, formatServerReply(fd, "401 " + client.nickname + " " + target + " :No such nick"));
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
        ClientInfo& client = _clients[fd];
        sendReply(fd, formatServerReply(fd, "409 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :No origin specified"));
        return;
    }
    
    // Respond with PONG
    sendReply(fd, ":" + SERVER_NAME + " PONG " + SERVER_NAME + " :" + params[0] + "\r\n");
}

void Server::handlePart(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " PART :Not enough parameters"));
        return;
    }

    std::string channel = params[0];
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    if (client.channels.find(channel) == client.channels.end()) {
        sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    std::string part_msg = "Leaving";
    if (params.size() > 1) {
        part_msg = params[1];
    }
    
    client.channels.erase(channel);
    _channels[channel].members.erase(fd);
    _channels[channel].operators.erase(fd);
    
    std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PART " + channel + " :" + part_msg + "\r\n";
    broadcastToChannel(channel, msg, -1); // Send to all including the one leaving
    
    if (_channels[channel].members.empty()) {
        _channels.erase(channel);
    }
    
    // std::cout << "Client " << client.nickname << " left " << channel << std::endl;
}

void Server::handleMode(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " MODE :Not enough parameters"));
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#') {
        // Channel mode
        std::map<std::string, ChannelInfo>::iterator chanIt = _channels.find(target);
        if (chanIt == _channels.end()) {
            sendReply(fd, formatServerReply(fd, "403 " + client.nickname + " " + target + " :No such channel"));
            return;
        }
        
        ChannelInfo& channel = chanIt->second;
        
        // Check if user is in the channel
        if (!isClientInChannel(target, fd)) {
            sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + target + " :You're not on that channel"));
            return;
        }
        
        // If no mode string provided, just show current modes
        if (params.size() == 1) {
            std::string modeStr = channel.getModeString();
            std::string response = "324 " + client.nickname + " " + target + " " + modeStr;
            
            // Add key if present
            if (!channel.key.empty()) {
                response += " " + channel.key;
            }
            // Add limit if present
            if (channel.userLimit > 0) {
                std::ostringstream oss;
                oss << channel.userLimit;
                response += " " + oss.str();
            }
            sendReply(fd, formatServerReply(fd, response));
            return;
        }
        
        // First check if mode string contains only supported modes
        std::string modeStr = params[1];
        bool hasSupportedModes = false;
        for (size_t i = 0; i < modeStr.length(); ++i) {
            char mode = modeStr[i];
            if (mode == 'i' || mode == 't' || mode == 'k' || mode == 'l' || mode == 'o') {
                hasSupportedModes = true;
                break;
            }
        }
        
        // If no supported modes found, silently ignore
        if (!hasSupportedModes) {
            return;
        }
        
        // Check if user is channel operator (only for supported modes)
        if (!isChannelOperator(target, fd)) {
            sendReply(fd, formatServerReply(fd, "482 " + client.nickname + " " + target + " :You're not channel operator"));
            return;
        }
        
        // Parse mode changes
        bool adding = true;
        size_t paramIndex = 2;
        
        std::string modeChanges = "";
        std::string modeParams = "";
        
        for (size_t i = 0; i < modeStr.length(); ++i) {
            char mode = modeStr[i];
            
            if (mode == '+') {
                adding = true;
                if (modeChanges.empty() || modeChanges[modeChanges.length() - 1] != '+') {
                    modeChanges += "+";
                }
            } else if (mode == '-') {
                adding = false;
                if (modeChanges.empty() || modeChanges[modeChanges.length() - 1] != '-') {
                    modeChanges += "-";
                }
            } else if (mode == 'i') {
                // Invite-only mode
                channel.inviteOnly = adding;
                modeChanges += "i";
            } else if (mode == 't') {
                // Topic restriction mode
                channel.topicRestricted = adding;
                modeChanges += "t";
            } else if (mode == 'k') {
                // Channel key (password)
                if (adding && paramIndex < params.size()) {
                    channel.key = params[paramIndex];
                    modeChanges += "k";
                    if (!modeParams.empty()) modeParams += " ";
                    modeParams += params[paramIndex];
                    paramIndex++;
                } else if (!adding) {
                    channel.key = "";
                    modeChanges += "k";
                }
            } else if (mode == 'l') {
                // User limit
                if (adding && paramIndex < params.size()) {
                    int limit;
                    if (stringToInt(params[paramIndex], limit) && limit > 0) {
                        channel.userLimit = static_cast<size_t>(limit);
                        modeChanges += "l";
                        if (!modeParams.empty()) modeParams += " ";
                        modeParams += params[paramIndex];
                        paramIndex++;
                    }
                } else if (!adding) {
                    channel.userLimit = 0;
                    modeChanges += "l";
                }
            } else if (mode == 'o') {
                // Operator privilege
                if (paramIndex < params.size()) {
                    std::string targetNick = params[paramIndex];
                    int targetFd = getClientFdByNick(targetNick);
                    
                    if (targetFd != -1 && isClientInChannel(target, targetFd)) {
                        if (adding) {
                            channel.operators.insert(targetFd);
                        } else {
                            channel.operators.erase(targetFd);
                        }
                        modeChanges += "o";
                        if (!modeParams.empty()) modeParams += " ";
                        modeParams += targetNick;
                    }
                    paramIndex++;
                }
            }
            // Silently ignore any other mode characters (like 'b', 'v', 'n', etc.)
        }
        
        // Broadcast mode change to channel
        if (!modeChanges.empty() && modeChanges != "+" && modeChanges != "-") {
            std::string modeMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                                " MODE " + target + " " + modeChanges;
            if (!modeParams.empty()) {
                modeMsg += " " + modeParams;
            }
            modeMsg += "\r\n";
            broadcastToChannel(target, modeMsg, -1);
        }
    } else {
        // User mode - we don't support user modes, just send empty mode string
        // sendReply(fd, "221 " + client.nickname + " +\r\n");
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
        std::map<std::string, ChannelInfo>::iterator it = _channels.find(target);
        if (it != _channels.end()) {
            for (std::set<int>::iterator client_it = it->second.members.begin(); 
                 client_it != it->second.members.end(); ++client_it) {
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
    
    for (std::map<std::string, ChannelInfo>::iterator it = _channels.begin(); 
         it != _channels.end(); ++it) {
        std::ostringstream oss;
        oss << it->second.members.size();
        sendReply(fd, "322 " + client.nickname + " " + it->first + " " + 
                 oss.str() + " :No topic\r\n");
    }
    
    sendReply(fd, "323 " + client.nickname + " :End of LIST\r\n");
}

void Server::handleKick(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " KICK :Not enough parameters"));
        return;
    }
    
    std::string channel = params[0];
    std::string targetNick = params[1];
    std::string reason = "Kicked";
    if (params.size() > 2) {
        reason = params[2];
    }
    
    // Ensure channel name starts with #
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    // Check if channel exists
    std::map<std::string, ChannelInfo>::iterator chanIt = _channels.find(channel);
    if (chanIt == _channels.end()) {
        sendReply(fd, formatServerReply(fd, "403 " + client.nickname + " " + channel + " :No such channel"));
        return;
    }
    
    ChannelInfo& chanInfo = chanIt->second;
    
    // Check if kicker is in the channel
    if (!isClientInChannel(channel, fd)) {
        sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    // Check if kicker is channel operator
    if (!isChannelOperator(channel, fd)) {
        sendReply(fd, formatServerReply(fd, "482 " + client.nickname + " " + channel + " :You're not channel operator"));
        return;
    }
    
    // Find target user
    int targetFd = getClientFdByNick(targetNick);
    if (targetFd == -1) {
        sendReply(fd, formatServerReply(fd, "401 " + client.nickname + " " + targetNick + " :No such nick/channel"));
        return;
    }
    
    // Check if target is in the channel
    if (!isClientInChannel(channel, targetFd)) {
        sendReply(fd, formatServerReply(fd, "441 " + client.nickname + " " + targetNick + " " + channel + " :They aren't on that channel"));
        return;
    }
    
    // Get target client info
    ClientInfo& targetClient = _clients[targetFd];
    
    // Broadcast KICK message to all channel members (including the kicked user)
    std::string kickMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                         " KICK " + channel + " " + targetNick + " :" + reason + "\r\n";
    broadcastToChannel(channel, kickMsg, -1);
    
    // Remove user from channel
    targetClient.channels.erase(channel);
    chanInfo.members.erase(targetFd);
    chanInfo.operators.erase(targetFd);
    
    // If channel is empty, delete it
    if (chanInfo.members.empty()) {
        _channels.erase(channel);
    }
}

void Server::handleInvite(int fd, const std::vector<std::string>& params) {
    ClientInfo& client = _clients[fd];
    if (!client.registered) {
        sendReply(fd, formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        sendReply(fd, formatServerReply(fd, "461 " + client.nickname + " INVITE :Not enough parameters"));
        return;
    }
    
    std::string targetNick = params[0];
    std::string channel = params[1];
    
    // Ensure channel name starts with #
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    // Check if channel exists
    std::map<std::string, ChannelInfo>::iterator chanIt = _channels.find(channel);
    if (chanIt == _channels.end()) {
        sendReply(fd, formatServerReply(fd, "403 " + client.nickname + " " + channel + " :No such channel"));
        return;
    }
    
    ChannelInfo& chanInfo = chanIt->second;
    
    // Check if inviter is in the channel
    if (!isClientInChannel(channel, fd)) {
        sendReply(fd, formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    // If channel is invite-only, only operators can invite
    if (chanInfo.inviteOnly && !isChannelOperator(channel, fd)) {
        sendReply(fd, formatServerReply(fd, "482 " + client.nickname + " " + channel + " :You're not channel operator"));
        return;
    }
    
    // Find target user
    int targetFd = getClientFdByNick(targetNick);
    if (targetFd == -1) {
        sendReply(fd, formatServerReply(fd, "401 " + client.nickname + " " + targetNick + " :No such nick/channel"));
        return;
    }
    
    // Check if target is already in the channel
    if (isClientInChannel(channel, targetFd)) {
        sendReply(fd, formatServerReply(fd, "443 " + client.nickname + " " + targetNick + " " + channel + " :is already on channel"));
        return;
    }
    
    // Add user to invite list
    chanInfo.invited.insert(targetFd);
    
    // Send invite notification to target user
    std::string inviteMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                           " INVITE " + targetNick + " :" + channel + "\r\n";
    sendReply(targetFd, inviteMsg);
    
    // Confirm to inviter
    sendReply(fd, formatServerReply(fd, "341 " + client.nickname + " " + targetNick + " " + channel));
}