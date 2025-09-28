// #include <iostream>
// #include "client.hpp"

// // Simple fake Channel class for testing
// class Channel {
// private:
//	 std::string name;
// public:
//	 Channel(std::string name) : name(name) {}
//	 std::string getChannelName() const { return name; }
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
//		 std::cout << " - " << (*it)->getChannelName() << std::endl;
//	 }

//	 // Leave one channel
//	 c1.leaveChannel(&ch1);

//	 std::cout << "Channels after leaving 'general': " << std::endl;
//	 channels = c1.getChannels();
//	 for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
//		 std::cout << " - " << (*it)->getChannelName() << std::endl;
//	 }

//	 return 0;
// }

#include "client.hpp"
#include "channel.hpp"
#include <iostream>
#include <vector>

int main() {
    // Create clients
    Client* alice = new Client("Alice", "alice_u", "Alice Real", 1, 1, true);
    Client* bob = new Client("Bob", "bob_u", "Bob Real", 2, 2, true);
    Client* charlie = new Client("Charlie", "charlie_u", "Charlie Real", 3, 3, true);

    // Create channel #general
    Channel* general = new Channel("#general");

    // Add clients to channel
    general->addClient(alice);
    general->addClient(bob);
    general->addClient(charlie);

    // Display clients
    std::cout << "Clients in " << general->getChannelName() << ": ";
    std::vector<Client*> clients = general->getClients();
    for (size_t i = 0; i < clients.size(); i++)
        std::cout << clients[i]->getNickname() << " ";
    std::cout << "\n\n";

    // Broadcast test
    std::cout << "Broadcast test (message from Alice):\n";
    general->broadcast("Hello everyone!", alice);
    std::cout << "\n";

    // Topic test (operator-only)
    general->setTopic("Initial Topic");
    std::cout << "Set topic by non-operator (Bob): ";
    general->setTopic("New Topic by Bob", bob); // Bob is not operator, should fail
    std::cout << general->getTopic() << "\n";
    std::cout << "Set topic by operator (Alice): ";
    general->setTopic("New Topic by Alice", alice); // Alice is operator
    std::cout << general->getTopic() << "\n\n";

    // User limit test
    general->setUserLimit(2);
    Client* david = new Client("David", "david_u", "David Real", 4, 4, true);
    general->addClient(david); // Should not add (limit reached)
    std::cout << "Clients in " << general->getChannelName() << " after adding David: ";
    clients = general->getClients();
    for (size_t i = 0; i < clients.size(); i++)
        std::cout << clients[i]->getNickname() << " ";
    std::cout << "\n\n";

    // Operator test
    general->addOperator(bob);
    std::cout << "Operators in " << general->getChannelName() << ": ";
    std::vector<Client*> operators = general->getOperators();
    for (size_t i = 0; i < operators.size(); i++)
        std::cout << operators[i]->getNickname() << " ";
    std::cout << "\n\n";

    // Remove a client
    general->removeClient(charlie);
    std::cout << "Clients in " << general->getChannelName() << " after Charlie left: ";
    clients = general->getClients();
    for (size_t i = 0; i < clients.size(); i++)
        std::cout << clients[i]->getNickname() << " ";
    std::cout << "\n\n";

    // Cleanup
    delete alice;
    delete bob;
    delete charlie;
    delete david;
    delete general;

    return 0;
}
