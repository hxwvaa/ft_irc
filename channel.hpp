#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <stdlib.h>
#include <vector>
#include <algorithm>

class Client;

class Channel {
	private:
		std::string channel_name;
		std::string topic;
		std::vector<Client *> clients;
		std::vector<Client*> operators;

	public:
		Channel(std::string const &channel_name);
		~Channel();

		std::string getChannelName() const;
		std::string getTopic() const;
		void setTopic(std::string const &topic);

		void addClient(Client *client);
		void removeClient(Client *client);
		std::vector<Client *> getClients() const;
		std::vector<Client*> getOperators() const;

		bool hasMember(Client* client) const;

		void addOperator(Client* client);
		void removeOperator(Client* client);
		bool isOperator(Client* client) const;

		void broadcast(std::string const &message, Client *sender);
};

#include "client.hpp"

#endif