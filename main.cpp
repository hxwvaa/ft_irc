// #include <iostream>
// #include "client.hpp"

// // Simple fake Channel class for testing
// class Channel {
// private:
//	 std::string name;
// public:
//	 Channel(std::string name) : name(name) {}
//	 std::string getName() const { return name; }
// };

// int main() {
//	 // Create client (always unregistered at start)
//	 Client c1("Adhil", "adhil_user", "Adhil Real", 5, 10, false);

//	 // Print initial values
//	 std::cout << "Nickname: " << c1.getNickname() << std::endl;
//	 std::cout << "Username: " << c1.getUsername() << std::endl;
//	 std::cout << "Realname: " << c1.getRealname() << std::endl;
//	 std::cout << "registered? " << (c1.isregistered() ? "Yes" : "No") << std::endl;

//	 // Update registration
//	 c1.setregistered(true);
//	 std::cout << "registered after update? " << (c1.isregistered() ? "Yes" : "No") << std::endl;

//	 // Test channel join/leave
//	 Channel ch1("general");
//	 Channel ch2("random");

//	 c1.joinChannel(&ch1);
//	 c1.joinChannel(&ch2);

//	 std::cout << "Channels joined: " << std::endl;
//	 std::vector<Channel*> channels = c1.getChannels();
//	 for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
//		 std::cout << " - " << (*it)->getName() << std::endl;
//	 }

//	 // Leave one channel
//	 c1.leaveChannel(&ch1);

//	 std::cout << "Channels after leaving 'general': " << std::endl;
//	 channels = c1.getChannels();
//	 for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
//		 std::cout << " - " << (*it)->getName() << std::endl;
//	 }

//	 return 0;
// }

#include "client.hpp"
#include "channel.hpp"
#include <iostream>

int main() {
	// Create clients
	Client alice("Alice", "alice123", "Alice Smith", 1, 0, true);
	Client bob("Bob", "bobby", "Bob Johnson", 2, 0, true);
	Client charlie("Charlie", "charlieX", "Charlie Brown", 3, 0, true);

	// Create a channel
	Channel general("#general");

	// Clients join the channel
	general.addClient(&alice);
	alice.joinChannel(&general);

	general.addClient(&bob);
	bob.joinChannel(&general);

	general.addClient(&charlie);
	charlie.joinChannel(&general);

	// Print clients in channel
	std::cout << "Clients in " << general.getChannelName() << ": ";
	std::vector<Client*> clients = general.getClients();
	for (std::vector<Client*>::size_type i = 0; i < clients.size(); ++i) {
		std::cout << clients[i]->getNickname() << " ";
	}
	std::cout << std::endl;

	// Broadcast a message from Alice
	std::cout << "\nBroadcast test (message from Alice):\n";
	for (std::vector<Client*>::size_type i = 0; i < clients.size(); ++i) {
		if (clients[i] != &alice) {
			std::cout << clients[i]->getNickname() 
					  << " receives: Hello everyone!" << std::endl;
		}
	}

	// Check if Bob is in the channel
	bool bobInChannel = general.hasMember(&bob);
	std::cout << "\nIs Bob in " << general.getChannelName() << "? " 
			  << (bobInChannel ? "Yes" : "No") << std::endl;

	// Remove Charlie from the channel
	general.removeClient(&charlie);
	charlie.leaveChannel(&general);

	// Print clients after Charlie leaves
	std::cout << "\nClients in " << general.getChannelName() << " after Charlie left: ";
	clients = general.getClients();
	for (std::vector<Client*>::size_type i = 0; i < clients.size(); ++i) {
		std::cout << clients[i]->getNickname() << " ";
	}
	std::cout << std::endl;

	return 0;
}