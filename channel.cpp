#include "channel.hpp"
#include <iostream>


Channel::Channel(std::string const &channel_name): channel_name(channel_name) {}

Channel::~Channel(){}

std::string Channel::getChannelName() const 
{
	return channel_name;
}

std::string Channel::getTopic() const{
	return topic;
}

void Channel::setTopic(std::string const &topic)
{
	this->topic = topic;
}

void Channel::addClient(Client *client)
{
	if (std::find(clients.begin(), clients.end(), client) == clients.end())
	{
		clients.push_back(client);
		client->joinChannel(this);
	}
}

void Channel::removeClient(Client *client)
{
	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
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

void Channel::broadcast(std::string const &message, Client *sender) {
	for (size_t i = 0; i < clients.size(); i++) {
		if (clients[i] != sender) {
			// just printing it now, late will integrate the sockets from Hamdhan
			std::cout << "[" << channel_name << "] " 
					<< sender->getNickname() << ": " 
					<< message << std::endl;
		}
	}
}