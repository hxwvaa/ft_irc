#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include <map>
#include "Message.hpp"
#include <functional>
#include "Channel.hpp"

class Client;

class Server {
public:
    typedef void (Server::*CommandFunction)(Client &, const Message &);

    Server(int port, const std::string &password);
    ~Server();

    void run();

private:
    int _port;
    std::string _password;
    int _sockfd;
    std::vector<struct pollfd> _fds;
    std::map<int, Client> _clients;
    std::map<std::string, Channel> _channels;
    std::map<std::string, CommandFunction> _commands;

    void setup();
    void handleNewConnection();
    void handleClientData(int fd);
    void removeClient(int fd);
    void processMessage(Client &client, const std::string &message);
    void sendReply(int fd, const std::string &reply);

    // Command handlers
    void pass(Client &client, const Message &message);
    void nick(Client &client, const Message &message);
    void user(Client &client, const Message &message);
    void join(Client &client, const Message &message);
    void privmsg(Client &client, const Message &message);

    static void signalHandler(int signum);
};

#endif // SERVER_HPP
