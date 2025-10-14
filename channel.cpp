#include "channel.hpp"
#include <iostream>

Channel::Channel(const std::string& name)
	: channel_name(name), key(""), userLimit(0), inviteOnly(false), topicProtected(false) {}

Channel::~Channel(){}

std::string Channel::getChannelName() const 
{
	return channel_name;
}

std::string Channel::getTopic() const{
	return topic;
}

void Channel::setTopic(const std::string& t, Client* setter) {
	if (topicProtected && setter != NULL && !isOperator(setter)) {
		std::cout << setter->getNickname() << " is not operator and cannot set topic\n";
		return;
	}
	topic = t;
}

void Channel::setKey(const std::string& k) {
	this->key = k;
}

std::string Channel::getKey() const {
	return key;
}

void Channel::setUserLimit(size_t limit) {
	userLimit = limit;
}

size_t Channel::getUserLimit() const {
	return userLimit;
}

void Channel::setInviteOnly(bool mode) {
	inviteOnly = mode;
}

bool Channel::isInviteOnly() const {
	return inviteOnly;
}

void Channel::setTopicProtected(bool mode) {
	topicProtected = mode;
}

bool Channel::isTopicProtected() const {
	return topicProtected;
}

void Channel::addInvited(Client* client) {
	invited.insert(client);
}

void Channel::removeInvited(Client* client) {
	invited.erase(client);
}

bool Channel::isInvited(Client* client) const {
	return invited.find(client) != invited.end();
}

void Channel::addClient(Client* client) {
	if (isInviteOnly() && !isInvited(client)) {
		std::cout << client->getNickname() << " cannot join, channel is invite-only and client not invited\n";
		return;
	}
	if (!key.empty()) {
		// Key checking will be done at server level
		std::cout << client->getNickname() << " would need key to join\n";
	}
	if (userLimit != 0 && clients.size() >= userLimit) {
		std::cout << client->getNickname() << " cannot join, channel is full\n";
		return;
	}
	if (!hasMember(client)) {
		clients.push_back(client);
		client->joinChannel(this);
		if (clients.size() == 1) addOperator(client); // first client is op
		removeInvited(client); // Remove from invite list after joining
	}
}

void Channel::removeClient(Client *client)
{
	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
	removeOperator(client);
	removeInvited(client);
	client->leaveChannel(this);
}

std::vector<Client *> Channel::getClients() const
{
	return clients;
}

std::vector<Client*> Channel::getOperators() const {
	return operators;
}

bool Channel::hasMember(Client* client) const {
	return std::find(clients.begin(), clients.end(), client) != clients.end();
}

void Channel::addOperator(Client* client) {
	if (!isOperator(client))
		operators.push_back(client);
}

void Channel::removeOperator(Client* client) {
	std::vector<Client*>::iterator it = std::find(operators.begin(), operators.end(), client);
	if (it != operators.end())
		operators.erase(it);
}

bool Channel::isOperator(Client* client) const {
	return std::find(operators.begin(), operators.end(), client) != operators.end();
}

void Channel::broadcast(const std::string& message, Client* sender) {
	for (size_t i = 0; i < clients.size(); ++i) {
		if (clients[i] != sender) {
			clients[i]->sendMessage(message);
		}
	}
}