#include "Server.hpp"
#include "parcer.hpp"
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

std::string ChannelInfo::getModeString() const {
    std::string modes = "+";
    if (inviteOnly) modes += "i";
    if (topicRestricted) modes += "t";
    if (!key.empty()) modes += "k";
    if (userLimit > 0) modes += "l";
    return modes;
}

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

    // Handle IRC commands
    if (command == "PASS") {
        ::handlePass(this, fd, params);
    } else if (command == "NICK") {
        ::handleNick(this, fd, params);
    } else if (command == "USER") {
        ::handleUser(this, fd, params);
    } else if (command == "JOIN") {
        ::handleJoin(this, fd, params);
    } else if (command == "PRIVMSG") {
        ::handlePrivMsg(this, fd, params);
    } else if (command == "QUIT") {
        ::handleQuit(this, fd, params);
    } else if (command == "PING") {
        ::handlePing(this, fd, params);
    } else if (command == "PART") {
        ::handlePart(this, fd, params);
    } else if (command == "MODE") {
        ::handleMode(this, fd, params);
    } else if (command == "WHO") {
        ::handleWho(this, fd, params);
    } else if (command == "LIST") {
        ::handleList(this, fd, params);
    } else if (command == "KICK") {
        ::handleKick(this, fd, params);
    } else if (command == "INVITE") {
        ::handleInvite(this, fd, params);
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

// Helper function: Get client by fd
ClientInfo& Server::getClient(int fd) {
    return _clients[fd];
}

// Helper function: Get channel by name
ChannelInfo& Server::getChannel(const std::string& name) {
    return _channels[name];
}
