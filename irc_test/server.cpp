#include "server.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

// Define static empty string member
const std::string Server::_emptyString = "";

Server::Server(const std::string& name, const std::string& password) 
    : _serverName(name), _serverPassword(password) {}

Server::~Server() {
    // Clean up clients
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
    }
    _clients.clear();
    _nickToFd.clear();
    
    // Clean up channels
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        delete it->second;
    }
    _channels.clear();
    _invites.clear();
}

// Helper methods
Client* Server::getClient(int fd) const {
    std::map<int, Client*>::const_iterator it = _clients.find(fd);
    return (it != _clients.end()) ? it->second : NULL;
}

Client* Server::getClientByNick(const std::string& nick) const {
    std::map<std::string, int>::const_iterator it = _nickToFd.find(nick);
    if (it != _nickToFd.end()) {
        return getClient(it->second);
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& name) const {
    std::map<std::string, Channel*>::const_iterator it = _channels.find(name);
    return (it != _channels.end()) ? it->second : NULL;
}

Channel* Server::getOrCreateChannel(const std::string& name, int creatorFd) {
    Channel* channel = getChannel(name);
    if (!channel) {
        channel = new Channel(name);
        _channels[name] = channel;
        
        // Make creator operator
        Client* creator = getClient(creatorFd);
        if (creator) {
            channel->addOperator(creator);
        }
    }
    return channel;
}

// Client management
void Server::addClient(int fd, const std::string& hostname) {
    if (_clients.find(fd) != _clients.end()) {
        return; // Client already exists
    }
    
    Client* client = new Client("", "", "", fd, fd, false);
    client->setHostname(hostname);
    _clients[fd] = client;
    
    std::cout << "Client connected with fd: " << fd << std::endl;
}

void Server::removeClient(int fd) {
    Client* client = getClient(fd);
    if (!client) return;
    
    std::cout << "Removing client: " << client->getNickname() << " (fd: " << fd << ")" << std::endl;
    
    // Remove from all channels
    std::vector<Channel*> clientChannels = client->getChannels();
    for (size_t i = 0; i < clientChannels.size(); ++i) {
        Channel* channel = clientChannels[i];
        channel->removeClient(client);
        
        // Remove channel if empty
        if (channel->getClients().empty()) {
            _channels.erase(channel->getChannelName());
            delete channel;
        }
    }
    
    // Remove nickname mapping
    if (!client->getNickname().empty()) {
        _nickToFd.erase(client->getNickname());
    }
    
    // Remove from invite lists
    for (std::map<std::string, std::set<int> >::iterator it = _invites.begin(); it != _invites.end(); ++it) {
        it->second.erase(fd);
    }
    
    // Remove client
    delete client;
    _clients.erase(fd);
}

void Server::processMessage(int fd, const std::string& message) {
    Parcer parser(this);
    IRCCommand cmd = parser.parse(message);
    
    std::cout << "Processing message from fd " << fd << ": " << message << std::endl;
    
    if (!cmd.command.empty()) {
        parser.handle(fd, cmd);
    }
}

// Test helper methods
void Server::printClientInfo(int fd) const {
    Client* client = getClient(fd);
    if (client) {
        std::cout << "Client fd " << fd << ": nick='" << client->getNickname() 
                  << "', user='" << client->getUsername() 
                  << "', real='" << client->getRealname() 
                  << "', registered=" << client->isregistered()
                  << ", auth=" << client->isAuthenticated() << std::endl;
    } else {
        std::cout << "No client with fd " << fd << std::endl;
    }
}

void Server::printChannelInfo(const std::string& channelName) const {
    Channel* channel = getChannel(channelName);
    if (channel) {
        std::cout << "Channel " << channelName << ": topic='" << channel->getTopic()
                  << "', members=" << channel->getClients().size() 
                  << ", operators=" << channel->getOperators().size() 
                  << ", inviteOnly=" << channel->isInviteOnly() 
                  << ", topicProtected=" << channel->isTopicProtected() << std::endl;
        
        std::vector<Client*> members = channel->getClients();
        for (size_t i = 0; i < members.size(); ++i) {
            std::cout << "  - " << members[i]->getNickname();
            if (channel->isOperator(members[i])) {
                std::cout << " (operator)";
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << "No channel named " << channelName << std::endl;
    }
}

void Server::listAllClients() const {
    std::cout << "=== All Clients ===" << std::endl;
    for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        printClientInfo(it->first);
    }
}

void Server::listAllChannels() const {
    std::cout << "=== All Channels ===" << std::endl;
    for (std::map<std::string, Channel*>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
        printChannelInfo(it->first);
    }
}

// ServerAdapter interface implementation
const std::string& Server::serverName() const { 
    return _serverName; 
}

const std::string& Server::serverPassword() const { 
    return _serverPassword; 
}

bool Server::hasClient(int fd) const { 
    return _clients.find(fd) != _clients.end(); 
}

bool Server::isRegistered(int fd) const {
    Client* client = getClient(fd);
    return client && client->isregistered();
}

void Server::setRegistered(int fd, bool v) {
    Client* client = getClient(fd);
    if (client) client->setRegistered(v);
}

const std::string& Server::getNick(int fd) const {
    Client* client = getClient(fd);
    if (client) {
        // Store the result in a static variable to return reference
        static std::string result;
        result = client->getNickname();
        return result;
    }
    return _emptyString;
}

void Server::setNick(int fd, const std::string& nick) {
    Client* client = getClient(fd);
    if (client) {
        // Remove old nickname mapping
        if (!client->getNickname().empty()) {
            _nickToFd.erase(client->getNickname());
        }
        
        // Set new nickname and update mapping
        client->setNickname(nick);
        if (!nick.empty()) {
            _nickToFd[nick] = fd;
        }
    }
}

const std::string& Server::getUser(int fd) const {
    Client* client = getClient(fd);
    if (client) {
        static std::string result;
        result = client->getUsername();
        return result;
    }
    return _emptyString;
}

void Server::setUser(int fd, const std::string& user) {
    Client* client = getClient(fd);
    if (client) client->setUsername(user);
}

const std::string& Server::getRealname(int fd) const {
    Client* client = getClient(fd);
    if (client) {
        static std::string result;
        result = client->getRealname();
        return result;
    }
    return _emptyString;
}

void Server::setRealname(int fd, const std::string& rn) {
    Client* client = getClient(fd);
    if (client) client->setRealname(rn);
}

bool Server::checkPasswordGiven(int fd) const {
    Client* client = getClient(fd);
    return client && client->isPasswordGiven();
}

void Server::setPasswordGiven(int fd, bool v) {
    Client* client = getClient(fd);
    if (client) client->setPasswordGiven(v);
}

bool Server::isOper(int fd) const {
    Client* client = getClient(fd);
    return client && client->isServerOper();
}

void Server::setOper(int fd, bool v) {
    Client* client = getClient(fd);
    if (client) client->setServerOper(v);
}

bool Server::isValidNick(const std::string& nick) const {
    // Create a local copy of the validation logic since the method is private in Parcer
    if (nick.empty() || nick.length() > 30) return false;
    
    // First char must be a letter
    char first = nick[0];
    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
        return false;
    
    // Allowed: letters, digits, _ - [ ] { } \ ` |
    for (size_t i = 1; i < nick.length(); ++i) {
        char c = nick[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '[' ||
              c == ']' || c == '{' || c == '}' || c == '\\' || c == '`' || c == '|')) {
            return false;
        }
    }
    return true;
}

int Server::fdByNick(const std::string& nick) const {
    std::map<std::string, int>::const_iterator it = _nickToFd.find(nick);
    return (it != _nickToFd.end()) ? it->second : -1;
}

bool Server::channelExists(const std::string& name) const {
    return _channels.find(name) != _channels.end();
}

void Server::createChannel(const std::string& name, int creatorFd) {
    getOrCreateChannel(name, creatorFd);
}

bool Server::isInChannel(int fd, const std::string& name) const {
    Client* client = getClient(fd);
    Channel* channel = getChannel(name);
    return client && channel && channel->hasMember(client);
}

void Server::joinChannel(int fd, const std::string& name) {
    Client* client = getClient(fd);
    Channel* channel = getOrCreateChannel(name, fd);
    if (client && channel) {
        channel->addClient(client);
    }
}

// FIXED: Use the reason parameter to avoid unused parameter warning
void Server::partChannel(int fd, const std::string& name, const std::string& reason) {
    (void)reason; // Mark as used to avoid warning
    Client* client = getClient(fd);
    Channel* channel = getChannel(name);
    if (client && channel) {
        channel->removeClient(client);
        
        // Remove channel if empty
        if (channel->getClients().empty()) {
            _channels.erase(name);
            delete channel;
        }
    }
}

bool Server::isChanOper(int fd, const std::string& name) const {
    Client* client = getClient(fd);
    Channel* channel = getChannel(name);
    return client && channel && channel->isOperator(client);
}

// FIXED: Use the fd parameter to avoid unused parameter warning
void Server::setChanOper(int fd, const std::string& name, int targetFd, bool v) {
    (void)fd; // Mark as used to avoid warning
    Client* targetClient = getClient(targetFd);
    Channel* channel = getChannel(name);
    if (targetClient && channel) {
        if (v) {
            channel->addOperator(targetClient);
        } else {
            channel->removeOperator(targetClient);
        }
    }
}

const std::set<int>& Server::channelMembers(const std::string& name) const {
    static std::set<int> empty;
    Channel* channel = getChannel(name);
    if (!channel) return empty;
    
    static std::set<int> members;
    members.clear();
    std::vector<Client*> clients = channel->getClients();
    for (size_t i = 0; i < clients.size(); ++i) {
        members.insert(clients[i]->getFd());
    }
    return members;
}

const std::string& Server::getTopic(const std::string& name) const {
    Channel* channel = getChannel(name);
    if (channel) {
        static std::string result;
        result = channel->getTopic();
        return result;
    }
    return _emptyString;
}

void Server::setTopic(const std::string& name, const std::string& topic) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setTopic(topic);
    }
}

bool Server::mode_i(const std::string& name) const {
    Channel* channel = getChannel(name);
    return channel && channel->isInviteOnly();
}

void Server::set_mode_i(const std::string& name, bool v) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setInviteOnly(v);
    }
}

bool Server::mode_t(const std::string& name) const {
    Channel* channel = getChannel(name);
    return channel && channel->isTopicProtected();
}

void Server::set_mode_t(const std::string& name, bool v) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setTopicProtected(v);
    }
}

bool Server::has_key(const std::string& name) const {
    Channel* channel = getChannel(name);
    return channel && !channel->getKey().empty();
}

const std::string& Server::key(const std::string& name) const {
    Channel* channel = getChannel(name);
    if (channel) {
        static std::string result;
        result = channel->getKey();
        return result;
    }
    return _emptyString;
}

void Server::set_key(const std::string& name, const std::string& k) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setKey(k);
    }
}

void Server::unset_key(const std::string& name) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setKey("");
    }
}

int Server::limit(const std::string& name) const {
    Channel* channel = getChannel(name);
    return channel ? channel->getUserLimit() : 0;
}

void Server::set_limit(const std::string& name, int n) {
    Channel* channel = getChannel(name);
    if (channel) {
        channel->setUserLimit(n);
    }
}

bool Server::isInvited(const std::string& name, int fd) const {
    std::map<std::string, std::set<int> >::const_iterator it = _invites.find(name);
    if (it != _invites.end()) {
        return it->second.find(fd) != it->second.end();
    }
    return false;
}

void Server::addInvite(const std::string& name, int fd) {
    _invites[name].insert(fd);
    
    // Also add to channel's invite list
    Channel* channel = getChannel(name);
    Client* client = getClient(fd);
    if (channel && client) {
        channel->addInvited(client);
    }
}

void Server::removeInvite(const std::string& name, int fd) {
    std::map<std::string, std::set<int> >::iterator it = _invites.find(name);
    if (it != _invites.end()) {
        it->second.erase(fd);
        if (it->second.empty()) {
            _invites.erase(it);
        }
    }
    
    // Also remove from channel's invite list
    Channel* channel = getChannel(name);
    Client* client = getClient(fd);
    if (channel && client) {
        channel->removeInvited(client);
    }
}

void Server::sendToFd(int fd, const std::string& msg) {
    Client* client = getClient(fd);
    if (client) {
        client->sendMessage(msg + "\r\n");
    }
}

void Server::sendNumeric(int fd, int code, const std::string& msgSuffix) {
    Client* client = getClient(fd);
    if (client) {
        std::stringstream ss;
        ss << ":" << _serverName << " " << code << " " << client->getNickname() << " " << msgSuffix << "\r\n";
        client->sendMessage(ss.str());
    }
}

void Server::broadcastToChannel(const std::string& name, const std::string& msg, int exceptFd) {
    Channel* channel = getChannel(name);
    if (channel) {
        Client* exceptClient = getClient(exceptFd);
        channel->broadcast(msg, exceptClient);
    }
}

void Server::noticeToFd(int fd, const std::string& msg) {
    sendToFd(fd, msg);
}

std::vector<std::string> Server::listChannels() const {
    std::vector<std::string> result;
    for (std::map<std::string, Channel*>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
        result.push_back(it->first);
    }
    return result;
}

size_t Server::channelSize(const std::string& name) const {
    Channel* channel = getChannel(name);
    return channel ? channel->getClients().size() : 0;
}