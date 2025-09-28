#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main() {
	// --- Step 1: Ask for nickname ---
	std::string nickname;
	while (true) {
		std::cout << "Input Nickname: ";
		std::getline(std::cin, nickname);
		if (nickname.length() > 0)
			break;
		std::cout << "You didn't input a nickname!" << std::endl;
	}

	// --- Step 2: Ask for username/realname ---
	std::string username;
	std::string realname;
	while (true) {
		std::cout << "Input Username: ";
		std::getline(std::cin, username);
		if (username.length() > 0)
			break;
		std::cout << "You didn't input a username!" << std::endl;
	}

	while (true) {
		std::cout << "Input Realname: ";
		std::getline(std::cin, realname);
		if (realname.length() > 0)
			break;
		std::cout << "You didn't input a realname!" << std::endl;
	}

	// --- Step 3: Create socket ---
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		std::cerr << "Socket creation failed!" << std::endl;
		return 1;
	}

	// --- Step 4: Setup server address ---
	struct sockaddr_in serv_addr;
	std::memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6667); // replace with your server port
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // localhost

	// --- Step 5: Connect to server ---
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		std::cerr << "Connection failed!" << std::endl;
		return 1;
	}
	std::cout << "Connected to server!" << std::endl;

	// --- Step 6: Send NICK command ---
	std::string nickCmd = "NICK " + nickname + "\r\n";
	send(sock, nickCmd.c_str(), nickCmd.size(), 0);

	// --- Step 7: Send USER command ---
	std::string userCmd = "USER " + username + " 0 * :" + realname + "\r\n";
	send(sock, userCmd.c_str(), userCmd.size(), 0);

	// --- Step 8: Read server response ---
	char buffer[1024];
	int valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (valread > 0) {
		buffer[valread] = '\0';
		std::cout << "Server says: " << buffer << std::endl;
	}

	// --- Step 10: Ask user for channel to join ---
	std::string channel;
	while (true) {
		std::cout << "Enter channel to join (start with #): ";
		std::getline(std::cin, channel);
		if (!channel.empty() && channel[0] == '#')
			break;
		std::cout << "Channel must start with '#'!" << std::endl;
	}

	// --- Step 11: Send JOIN command ---
	std::string joinCmd = "JOIN " + channel + "\r\n";
	send(sock, joinCmd.c_str(), joinCmd.size(), 0);

	// --- Step 12: Read server response ---
	valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (valread > 0) {
		buffer[valread] = '\0';
		std::cout << "Server says: " << buffer << std::endl;
	}

	// --- Step 13: Ask user for message to send ---
	std::string message;
	std::cout << "Enter message to send to " << channel << ": ";
	std::getline(std::cin, message);

	// --- Step 14: Send PRIVMSG command ---
	std::string privmsgCmd = "PRIVMSG " + channel + " :" + message + "\r\n";
	send(sock, privmsgCmd.c_str(), privmsgCmd.size(), 0);

	// --- Step 15: Read server response ---
	valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (valread > 0) {
		buffer[valread] = '\0';
		std::cout << "Server says: " << buffer << std::endl;
	}


	while (true) {
		// --- Read message from user ---
		std::string message;
		std::cout << "Enter message (or /quit to exit): ";
		std::getline(std::cin, message);

		if (message == "/quit") {
			std::cout << "Exiting..." << std::endl;
			break;
		}

		// --- Send message to channel ---
		std::string privmsgCmd = "PRIVMSG " + channel + " :" + message + "\r\n";
		send(sock, privmsgCmd.c_str(), privmsgCmd.size(), 0);

		// --- Read server response ---
		char buffer[1024];
		int valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (valread > 0) {
			buffer[valread] = '\0';
			std::cout << "Server says: " << buffer << std::endl;
		}
	}

	// --- Step 9: Close socket ---
	close(sock);

	return 0;
}
