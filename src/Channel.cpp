#include "Channel.hpp"
#include "Client.hpp"
#include <algorithm>
#include <sys/socket.h>

Channel::Channel() : _name("") {}

Channel::Channel(const std::string &name) : _name(name) {}
Channel::~Channel() {}

const std::string &Channel::getName() const {
    return _name;
}

const std::vector<Client*> &Channel::getClients() const {
    return _clients;
}

void Channel::addClient(Client *client) {
    if (std::find(_clients.begin(), _clients.end(), client) == _clients.end()) {
        _clients.push_back(client);
    }
}

void Channel::removeClient(Client *client) {
    std::vector<Client*>::iterator it = std::find(_clients.begin(), _clients.end(), client);
    if (it != _clients.end()) {
        _clients.erase(it);
    }
}

bool Channel::hasClient(Client *client) const {
    return std::find(_clients.begin(), _clients.end(), client) != _clients.end();
}

void Channel::broadcast(const std::string &message, Client *exclude) {
    for (std::vector<Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (*it != exclude) {
            send((*it)->getFd(), message.c_str(), message.length(), 0);
        }
    }
}
