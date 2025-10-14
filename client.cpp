#include "client.hpp"
#include <unistd.h>
#include <iostream>

Client::Client(std::string nickname, std::string username, std::string realname, int fd, int s_fd, bool registered)
	: nickname(nickname), username(username), realname(realname), fd(fd), s_fd(s_fd), 
	  registered(registered), authenticated(false), welcomeMsg(false), passwordGiven(false), isOper(false) {}

Client::Client(Client const &other) {
	*this = other;
}

Client &Client::operator=(Client const &other) {
	if (this != &other) {
		nickname = other.nickname;
		username = other.username;
		realname = other.realname;
		hostname = other.hostname;
		fd = other.fd;
		s_fd = other.s_fd;
		registered = other.registered;
		list_channels = other.list_channels;
		authenticated = other.authenticated;
		welcomeMsg = other.welcomeMsg;
		passwordGiven = other.passwordGiven;
		isOper = other.isOper;
	}
	return *this;
}

Client::~Client() {}

std::string Client::getNickname() const {
	return nickname;
}

std::string Client::getUsername() const {
	return username;
}

std::string Client::getRealname() const {
	return realname;
}

std::string Client::getHostname() const {
	return hostname;
}

int Client::getFd() const {
	return fd;
}

int Client::getS_fd() const {
	return s_fd;
}

void Client::setRegistered(bool status) {
	registered = status;
}

bool Client::isregistered() const{
	return registered;
}

bool Client::isAuthenticated() const {
	return authenticated;
}

void Client::setAuthenticated(bool status) {
	authenticated = status;
}

bool Client::hasWelcomeMsg() const {
	return welcomeMsg;
}

void Client::setWelcomeMsg(bool status) {
	welcomeMsg = status;
}

bool Client::isPasswordGiven() const {
	return passwordGiven;
}

void Client::setPasswordGiven(bool status) {
	passwordGiven = status;
}

bool Client::isServerOper() const {
	return isOper;
}

void Client::setServerOper(bool status) {
	isOper = status;
}

void Client::setNickname(const std::string& nick) {
	nickname = nick;
}

void Client::setUsername(const std::string& user) {
	username = user;
}

void Client::setRealname(const std::string& real) {
	realname = real;
}

void Client::setHostname(const std::string& host) {
	hostname = host;
}

void Client::joinChannel(Channel* channel) {
	if (!isInChannel(channel)) {
		list_channels.push_back(channel);
	}
}

void Client::leaveChannel(Channel *channel) {
	this->list_channels.erase(
		std::remove(this->list_channels.begin(), this->list_channels.end(), channel),
		this->list_channels.end()
	);
}

const std::vector<Channel*> &Client::getChannels() const {
	return list_channels;
}

bool Client::isInChannel(Channel* channel) const {
	return std::find(list_channels.begin(), list_channels.end(), channel) != list_channels.end();
}

bool Client::hasClient(Client* client) const {
	for (std::vector<Channel*>::size_type i = 0; i < list_channels.size(); ++i) {
		if (list_channels[i]->hasMember(client))
			return true;
	}
	return false;
}

void Client::clearChannels() {
	list_channels.clear();
}

std::string Client::getPrefix() const {
	return nickname + "!" + username + "@" + hostname;
}

void Client::sendMessage(const std::string& message) const {
	if (fd > 0) {
		// In real implementation, this would write to socket
		// For testing, we'll print to console
		std::cout << "[" << fd << "->" << nickname << "] " << message;
		if (message.find("\r\n") == std::string::npos) {
			std::cout << std::endl;
		}
	} else {
		// For testing without socket
		std::cout << nickname << " receives: " << message;
		if (message.find("\r\n") == std::string::npos) {
			std::cout << std::endl;
		}
	}
}