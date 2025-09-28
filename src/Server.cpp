#include "Server.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include <iostream>
#include <sstream>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

static bool g_server_running = true;

void Server::signalHandler(int signum) {
    (void)signum;
    g_server_running = false;
}

Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _sockfd(-1) {
    std::cout << "Server created with port " << _port << std::endl;
    signal(SIGINT, Server::signalHandler);
    signal(SIGQUIT, Server::signalHandler);

    // Register commands
    _commands["PASS"] = &Server::pass;
    _commands["NICK"] = &Server::nick;
    _commands["USER"] = &Server::user;
    _commands["JOIN"] = &Server::join;
    _commands["PRIVMSG"] = &Server::privmsg;
    _commands["NICK"] = &Server::nick;
}

Server::~Server() {
    // Cleanup code if needed
}

void Server::run() {
    setup();

    std::cout << "Server listening on port " << _port << std::endl;

    while (g_server_running) {
        if (poll(_fds.data(), _fds.size(), -1) < 0 && g_server_running) {
            std::cerr << "Error in poll" << std::endl;
            break;
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
    close(_sockfd);
}

void Server::setup() {
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        throw std::runtime_error("Error creating socket");
    }

    int opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Error setting socket options");
    }

    if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0) {
        throw std::runtime_error("Error setting socket to non-blocking");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_port);

    if (bind(_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Error binding socket");
    }

    if (listen(_sockfd, 5) < 0) {
        throw std::runtime_error("Error listening on socket");
    }

    _fds.push_back((struct pollfd){_sockfd, POLLIN, 0});
}

void Server::handleNewConnection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(_sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Error accepting new connection" << std::endl;
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error setting client socket to non-blocking" << std::endl;
        close(client_fd);
        return;
    }

    _fds.push_back((struct pollfd){client_fd, POLLIN, 0});
    _clients.insert(std::make_pair(client_fd, Client(client_fd)));
    std::cout << "New connection from client with fd " << client_fd << std::endl;
}

void Server::handleClientData(int fd) {
    char buffer[512];
    ssize_t nbytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (nbytes <= 0) {
        if (nbytes == 0) {
            std::cout << "Client with fd " << fd << " disconnected." << std::endl;
        } else {
            if (g_server_running)
                std::cerr << "Error receiving data from client " << fd << std::endl;
        }
        removeClient(fd);
    } else {
        buffer[nbytes] = '\0';
        Client &client = _clients.at(fd);
        client._buffer += buffer;
        
        size_t pos;
        while ((pos = client._buffer.find("\r\n")) != std::string::npos) {
            std::string line = client._buffer.substr(0, pos);
            client._buffer.erase(0, pos + 2);
            if (!line.empty()) {
                processMessage(client, line);
            }
        }
    }
}

void Server::processMessage(Client &client, const std::string &line) {
    std::cout << "<- " << line << std::endl;
    Message message(line);
    if (message.getCommand().empty()) return;

    if (!client.isAuthenticated() && message.getCommand() != "PASS" && message.getCommand() != "NICK" && message.getCommand() != "USER") {
        sendReply(client.getFd(), "451 :You have not registered\r\n");
        return;
    }

    std::map<std::string, CommandFunction>::iterator it = _commands.find(message.getCommand());
    if (it != _commands.end()) {
        (this->*(it->second))(client, message);
    } else {
        sendReply(client.getFd(), "421 " + message.getCommand() + " :Unknown command\r\n");
    }
}

void Server::sendReply(int fd, const std::string &reply) {
    std::cout << "-> " << reply;
    if (send(fd, reply.c_str(), reply.length(), 0) < 0) {
        std::cerr << "Error sending reply to client " << fd << std::endl;
    }
}

void Server::pass(Client &client, const Message &message) {
    if (client.isRegistered()) {
        sendReply(client.getFd(), "462 :You may not reregister\r\n");
        return;
    }
    if (message.getParams().empty()) {
        sendReply(client.getFd(), "461 PASS :Not enough parameters\r\n");
        return;
    }
    if (message.getParams()[0] == _password) {
        client.setAuthenticated(true);
        std::cout << "Client " << client.getFd() << " authenticated." << std::endl;
        client.checkRegistration();
    } else {
        sendReply(client.getFd(), "464 :Password incorrect\r\n");
    }
}

void Server::nick(Client &client, const Message &message) {
    if (message.getParams().empty() || message.getParams()[0].empty()) {
        sendReply(client.getFd(), "431 :No nickname given\r\n");
        return;
    }
    std::string new_nick = message.getParams()[0];

    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.getNickname() == new_nick) {
            sendReply(client.getFd(), "433 " + new_nick + " :Nickname is already in use\r\n");
            return;
        }
    }

    std::string old_nick = client.getNickname();
    client.setNickname(new_nick);
    if (client.isRegistered() && !old_nick.empty()) {
        std::string nick_change_msg = ":" + old_nick + " NICK :" + new_nick + "\r\n";
        sendReply(client.getFd(), nick_change_msg);
    }
    std::cout << "Client " << client.getFd() << " set nickname to " << new_nick << std::endl;
    client.checkRegistration();
}

void Server::user(Client &client, const Message &message) {
    if (client.isRegistered()) {
        sendReply(client.getFd(), "462 :You may not reregister\r\n");
        return;
    }
    if (message.getParams().size() < 4) {
        sendReply(client.getFd(), "461 USER :Not enough parameters\r\n");
        return;
    }
    client.setUsername(message.getParams()[0]);
    client.setRealname(message.getParams()[3]);
    std::cout << "Client " << client.getFd() << " set user details." << std::endl;
    client.checkRegistration();

    if (client.isRegistered()) {
        std::string welcome_msg = "001 " + client.getNickname() + " :Welcome to the IRC Network, " + client.getNickname() + "\r\n";
        sendReply(client.getFd(), welcome_msg);
    }
}

void Server::join(Client &client, const Message &message) {
    if (!client.isRegistered()) {
        sendReply(client.getFd(), "451 :You have not registered\r\n");
        return;
    }
    if (message.getParams().empty()) {
        sendReply(client.getFd(), "461 JOIN :Not enough parameters\r\n");
        return;
    }
    std::string channelName = message.getParams()[0];
    
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        _channels.insert(std::make_pair(channelName, Channel(channelName)));
        it = _channels.find(channelName);
    }
    
    it->second.addClient(&client);
    
    std::string join_msg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName + "\r\n";
    sendReply(client.getFd(), join_msg);

    std::string topic_msg = "332 " + client.getNickname() + " " + channelName + " :No topic is set\r\n";
    sendReply(client.getFd(), topic_msg);

    std::string names_list;
    const std::vector<Client*>& clients = it->second.getClients();
    for (size_t i = 0; i < clients.size(); ++i) {
        names_list += clients[i]->getNickname() + " ";
    }
    
    std::string names_reply = "353 " + client.getNickname() + " = " + channelName + " :" + names_list + "\r\n";
    sendReply(client.getFd(), names_reply);
    
    std::string end_of_names_reply = "366 " + client.getNickname() + " " + channelName + " :End of /NAMES list.\r\n";
    sendReply(client.getFd(), end_of_names_reply);

    it->second.broadcast(join_msg, &client);
}

void Server::privmsg(Client &client, const Message &message) {
    if (!client.isRegistered()) {
        sendReply(client.getFd(), "451 :You have not registered\r\n");
        return;
    }
    if (message.getParams().size() < 2) {
        sendReply(client.getFd(), "461 PRIVMSG :Not enough parameters\r\n");
        return;
    }
    std::string target = message.getParams()[0];
    std::string msg = message.getParams()[1];
    std::string full_msg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PRIVMSG " + target + " :" + msg + "\r\n";

    if (target[0] == '#') {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it != _channels.end()) {
            it->second.broadcast(full_msg, &client);
        } else {
            sendReply(client.getFd(), "403 " + client.getNickname() + " " + target + " :No such channel\r\n");
        }
    } else {
        // Private message to user
        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.getNickname() == target) {
                sendReply(it->second.getFd(), full_msg);
                return;
            }
        }
        sendReply(client.getFd(), "401 " + client.getNickname() + " " + target + " :No such nick/channel\r\n");
    }
}

void Server::removeClient(int fd) {
    // Find the client to be removed
    std::map<int, Client>::iterator client_it = _clients.find(fd);
    if (client_it == _clients.end()) {
        return; // Client not found
    }

    // Notify channels that the client is leaving
    for (std::map<std::string, Channel>::iterator channel_it = _channels.begin(); channel_it != _channels.end(); ++channel_it) {
        channel_it->second.removeClient(&client_it->second);
        std::string part_msg = ":" + client_it->second.getNickname() + " PART " + channel_it->first + "\r\n";
        channel_it->second.broadcast(part_msg, NULL);
    }

    // Remove client from the main list
    _clients.erase(client_it);

    // Remove the file descriptor from the poll set
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (_fds[i].fd == fd) {
            _fds.erase(_fds.begin() + i);
            break;
        }
    }

    close(fd);
    std::cout << "Client with fd " << fd << " removed." << std::endl;
}

void Server::nick(Client &client, const Message &message);
void Server::user(Client &client, const Message &message);
void Server::join(Client &client, const Message &message);
void Server::privmsg(Client &client, const Message &message);
