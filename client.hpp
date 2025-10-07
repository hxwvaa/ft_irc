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
		std::string hostname;
		int fd;
		int s_fd;
		bool registered;
		bool authenticated;
		bool welcomeMsg;
		std::vector<Channel*> list_channels;

	public:
		Client(std::string nickname, std::string username, std::string realname, int fd, int s_fd, bool registered);
		Client(Client const &other);
		Client &operator=(Client const &other);
		~Client();

		std::string getNickname() const;
		std::string getUsername() const;
		std::string getRealname() const;
		std::string getHostname() const;

		int getFd() const;
		int getS_fd() const;

		void setRegistered(bool status);
		bool isregistered() const;
		bool isAuthenticated() const;
		void setAuthenticated(bool status);
		bool hasWelcomeMsg() const;
		void setWelcomeMsg(bool status);

		bool hasClient(Client* client) const;

		void joinChannel(Channel *channel);
		void leaveChannel(Channel *channel);
		const std::vector<Channel*>& getChannels() const;
		bool isInChannel(Channel* channel) const;
		void clearChannels();

		std::string getPrefix() const;
		void sendMessage(const std::string& message) const;
};

#include "channel.hpp"

#endif
