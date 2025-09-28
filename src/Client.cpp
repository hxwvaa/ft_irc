#include "Client.hpp"

Client::Client() : _fd(-1), _authenticated(false), _registered(false) {}

Client::Client(int fd) : _fd(fd), _authenticated(false), _registered(false) {}

Client::~Client() {}

int Client::getFd() const {
    return _fd;
}

bool Client::isAuthenticated() const {
    return _authenticated;
}

void Client::setAuthenticated(bool authenticated) {
    _authenticated = authenticated;
}

const std::string &Client::getNickname() const {
    return _nickname;
}

void Client::setNickname(const std::string &nickname) {
    _nickname = nickname;
}

const std::string &Client::getUsername() const {
    return _username;
}

void Client::setUsername(const std::string &username) {
    _username = username;
}

const std::string &Client::getRealname() const {
    return _realname;
}

void Client::setRealname(const std::string &realname) {
    _realname = realname;
}

bool Client::isRegistered() const {
    return _registered;
}

void Client::checkRegistration() {
    if (!_registered && _authenticated && !_nickname.empty() && !_username.empty()) {
        _registered = true;
    }
}
