#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <algorithm>

class Channel;

class Client {
	private:
		std::string nickname;
		std::string username;
		std::string realname;
		int fd;
		int s_fd;
		bool registered;
		std::vector<Channel*> list_channels;
		std::vector<Client*> clients;

	public:
		Client(std::string nickname, std::string username, std::string realname, int fd, int s_fd, bool registered);
		Client(Client const &other);
		Client &operator=(Client const &other);
		~Client();

		std::string getNickname() const;
		std::string getUsername() const;
		std::string getRealname() const;

		int getFd() const;
		int getS_fd() const;

		void setRegistered(bool status);
		bool isregistered() const;

		bool hasClient(Client* client) const;

		void joinChannel(Channel *channel);
		void leaveChannel(Channel *channel);
		const std::vector<Channel*>& getChannels() const;
		bool isInChannel(Channel* channel) const;
		void clearChannels();

		void sendMessage(const std::string& message) const;
};

#include "channel.hpp"

#endif
