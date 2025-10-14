#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <set>

class Client;

class Channel {
	private:
		std::string channel_name;
		std::string topic;
		std::vector<Client *> clients;
		std::vector<Client*> operators;
		std::string key;
		size_t userLimit;
		bool inviteOnly;
		bool topicProtected;
		std::set<Client*> invited;

	public:
		Channel(std::string const &channel_name);
		~Channel();

		std::string getChannelName() const;
		std::string getTopic() const;
		void setTopic(const std::string& topic, Client* setter = NULL);

		void setKey(const std::string& key);
		std::string getKey() const;
		void setUserLimit(size_t limit);
		size_t getUserLimit() const;
		void setInviteOnly(bool mode);
		bool isInviteOnly() const;
		void setTopicProtected(bool mode);
		bool isTopicProtected() const;

		void addInvited(Client* client);
		void removeInvited(Client* client);
		bool isInvited(Client* client) const;

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