#include "server.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <iomanip>

// Forward declarations
void testBasicIRCFlow();
void testErrorHandling();
void testPerformance();

// Global server instance for interactive testing
Server* g_server = NULL;
std::map<int, std::string> g_clients; // fd -> nickname mapping for easy reference

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << " " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printHeader() {
    std::cout << "\033[2J\033[H"; // Clear screen
    std::cout << std::string(60, '*') << std::endl;
    std::cout << "*" << std::string(58, ' ') << "*" << std::endl;
    std::cout << "*" << std::setw(35) << "ft_irc Interactive Test Console" << std::string(23, ' ') << "*" << std::endl;
    std::cout << "*" << std::string(58, ' ') << "*" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
}

void showMenu() {
    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "                        MAIN MENU" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << " 1. Server Management" << std::endl;
    std::cout << " 2. Client Management" << std::endl;
    std::cout << " 3. Channel Operations" << std::endl;
    std::cout << " 4. Send IRC Commands" << std::endl;
    std::cout << " 5. View Server State" << std::endl;
    std::cout << " 6. Run Automated Tests" << std::endl;
    std::cout << " 7. Clear Screen" << std::endl;
    std::cout << " 0. Exit" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
}

void serverManagementMenu() {
    printSeparator("SERVER MANAGEMENT");
    
    if (g_server == NULL) {
        std::string serverName, password;
        std::cout << "No server running. Let's create one!" << std::endl;
        std::cout << "Enter server name (default: ft_irc): ";
        std::getline(std::cin, serverName);
        if (serverName.empty()) serverName = "ft_irc";
        
        std::cout << "Enter server password (default: testpass): ";
        std::getline(std::cin, password);
        if (password.empty()) password = "testpass";
        
        g_server = new Server(serverName, password);
        std::cout << "\n✅ Server '" << serverName << "' created successfully!" << std::endl;
        std::cout << "Password: " << password << std::endl;
    } else {
        std::cout << "Server is already running!" << std::endl;
        std::cout << "Server name: " << g_server->serverName() << std::endl;
        std::cout << "\nOptions:" << std::endl;
        std::cout << " r - Restart server" << std::endl;
        std::cout << " s - Server status" << std::endl;
        std::cout << " Enter - Back to main menu" << std::endl;
        
        std::string choice;
        std::cout << "\nChoice: ";
        std::getline(std::cin, choice);
        
        if (choice == "r") {
            delete g_server;
            g_server = NULL;
            g_clients.clear();
            std::cout << "Server stopped. Creating new server..." << std::endl;
            serverManagementMenu(); // Recursive call to create new server
        } else if (choice == "s") {
            std::cout << "\n📊 SERVER STATUS:" << std::endl;
            std::cout << "Name: " << g_server->serverName() << std::endl;
            std::cout << "Total Clients: " << g_clients.size() << std::endl;
        }
    }
}

void clientManagementMenu() {
    printSeparator("CLIENT MANAGEMENT");
    
    if (g_server == NULL) {
        std::cout << "❌ No server running! Please start server first." << std::endl;
        return;
    }
    
    std::cout << "Current clients:" << std::endl;
    if (g_clients.empty()) {
        std::cout << "  (No clients connected)" << std::endl;
    } else {
        for (std::map<int, std::string>::iterator it = g_clients.begin(); it != g_clients.end(); ++it) {
            std::cout << "  fd:" << it->first << " -> " << it->second << std::endl;
        }
    }
    
    std::cout << "\nOptions:" << std::endl;
    std::cout << " a - Add new client" << std::endl;
    std::cout << " r - Remove client" << std::endl;
    std::cout << " l - List all clients (detailed)" << std::endl;
    std::cout << " Enter - Back to main menu" << std::endl;
    
    std::string choice;
    std::cout << "\nChoice: ";
    std::getline(std::cin, choice);
    
    if (choice == "a") {
        int fd;
        std::string nick, user, real, hostname;
        
        std::cout << "\nEnter client fd (e.g., 10): ";
        std::cin >> fd;
        std::cin.ignore();
        
        std::cout << "Enter nickname: ";
        std::getline(std::cin, nick);
        
        std::cout << "Enter username (default: same as nick): ";
        std::getline(std::cin, user);
        if (user.empty()) user = nick;
        
        std::cout << "Enter real name (default: " << nick << " User): ";
        std::getline(std::cin, real);
        if (real.empty()) real = nick + " User";
        
        std::cout << "Enter hostname (default: localhost): ";
        std::getline(std::cin, hostname);
        if (hostname.empty()) hostname = "localhost";
        
        // Add client and register
        g_server->addClient(fd, hostname);
        g_server->processMessage(fd, "PASS " + g_server->serverPassword());
        g_server->processMessage(fd, "NICK " + nick);
        g_server->processMessage(fd, "USER " + user + " 0 * :" + real);
        
        g_clients[fd] = nick;
        std::cout << "\n✅ Client " << nick << " (fd:" << fd << ") added successfully!" << std::endl;
        
    } else if (choice == "r") {
        if (g_clients.empty()) {
            std::cout << "No clients to remove." << std::endl;
            return;
        }
        
        int fd;
        std::cout << "Enter client fd to remove: ";
        std::cin >> fd;
        std::cin.ignore();
        
        if (g_clients.find(fd) != g_clients.end()) {
            g_server->processMessage(fd, "QUIT :Removed by admin");
            g_clients.erase(fd);
            std::cout << "✅ Client removed." << std::endl;
        } else {
            std::cout << "❌ Client fd not found." << std::endl;
        }
        
    } else if (choice == "l") {
        g_server->listAllClients();
    }
}

void channelOperationsMenu() {
    printSeparator("CHANNEL OPERATIONS");
    
    if (g_server == NULL) {
        std::cout << "❌ No server running! Please start server first." << std::endl;
        return;
    }
    
    std::cout << "Options:" << std::endl;
    std::cout << " c - Create/Join channel" << std::endl;
    std::cout << " l - Leave channel" << std::endl;
    std::cout << " t - Set topic" << std::endl;
    std::cout << " m - Set channel mode" << std::endl;
    std::cout << " o - Make user operator" << std::endl;
    std::cout << " i - Invite user to channel" << std::endl;
    std::cout << " k - Kick user from channel" << std::endl;
    std::cout << " v - View all channels" << std::endl;
    std::cout << " Enter - Back to main menu" << std::endl;
    
    std::string choice;
    std::cout << "\nChoice: ";
    std::getline(std::cin, choice);
    
    if (choice == "c") {
        if (g_clients.empty()) {
            std::cout << "❌ No clients available. Add clients first." << std::endl;
            return;
        }
        
        int fd;
        std::string channel;
        std::cout << "Available clients:" << std::endl;
        for (std::map<int, std::string>::iterator it = g_clients.begin(); it != g_clients.end(); ++it) {
            std::cout << "  " << it->first << " -> " << it->second << std::endl;
        }
        std::cout << "Enter client fd: ";
        std::cin >> fd;
        std::cin.ignore();
        
        std::cout << "Enter channel name (with #): ";
        std::getline(std::cin, channel);
        
        g_server->processMessage(fd, "JOIN " + channel);
        
    } else if (choice == "t") {
        int fd;
        std::string channel, topic;
        std::cout << "Enter client fd: ";
        std::cin >> fd;
        std::cin.ignore();
        std::cout << "Enter channel name: ";
        std::getline(std::cin, channel);
        std::cout << "Enter topic: ";
        std::getline(std::cin, topic);
        
        g_server->processMessage(fd, "TOPIC " + channel + " :" + topic);
        
    } else if (choice == "v") {
        g_server->listAllChannels();
    }
    // Add other channel operations as needed
}

void sendCommandMenu() {
    printSeparator("SEND IRC COMMANDS");
    
    if (g_server == NULL) {
        std::cout << "❌ No server running! Please start server first." << std::endl;
        return;
    }
    
    if (g_clients.empty()) {
        std::cout << "❌ No clients available. Add clients first." << std::endl;
        return;
    }
    
    std::cout << "Available clients:" << std::endl;
    for (std::map<int, std::string>::iterator it = g_clients.begin(); it != g_clients.end(); ++it) {
        std::cout << "  " << it->first << " -> " << it->second << std::endl;
    }
    
    int fd;
    std::string command;
    
    std::cout << "\nEnter client fd: ";
    std::cin >> fd;
    std::cin.ignore();
    
    if (g_clients.find(fd) == g_clients.end()) {
        std::cout << "❌ Invalid client fd." << std::endl;
        return;
    }
    
    std::cout << "Enter IRC command (e.g., 'PRIVMSG #test :Hello world!'): ";
    std::getline(std::cin, command);
    
    if (!command.empty()) {
        std::cout << "\n📤 Sending command: " << command << std::endl;
        std::cout << "From: " << g_clients[fd] << " (fd:" << fd << ")" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        g_server->processMessage(fd, command);
    }
}

void viewServerState() {
    printSeparator("SERVER STATE");
    
    if (g_server == NULL) {
        std::cout << "❌ No server running!" << std::endl;
        return;
    }
    
    std::cout << "🖥️  SERVER INFO:" << std::endl;
    std::cout << "Name: " << g_server->serverName() << std::endl;
    std::cout << "Connected Clients: " << g_clients.size() << std::endl;
    
    std::cout << "\n👥 CLIENTS:" << std::endl;
    g_server->listAllClients();
    
    std::cout << "\n📺 CHANNELS:" << std::endl;
    g_server->listAllChannels();
}

void simulateClientSession(Server& server, int fd, const std::string& nick, 
                          const std::string& user, const std::string& realname, 
                          const std::string& hostname = "localhost") {
    std::cout << "\n>> Client " << nick << " connecting (fd: " << fd << ")..." << std::endl;
    
    // Add client to server
    server.addClient(fd, hostname);
    
    // IRC connection sequence: PASS -> NICK -> USER
    std::cout << ">> " << nick << " authenticating..." << std::endl;
    server.processMessage(fd, "PASS testpass");
    server.processMessage(fd, "NICK " + nick);
    server.processMessage(fd, "USER " + user + " 0 * :" + realname);
    
    std::cout << ">> " << nick << " registration complete!" << std::endl;
}

void testBasicIRCFlow() {
    printSeparator("BASIC IRC SERVER FUNCTIONALITY TEST");
    
    Server server("ft_irc", "testpass");
    
    // Test 1: Multiple client connections
    printSeparator("TEST 1: Client Registration");
    simulateClientSession(server, 4, "alice", "alice", "Alice Wonderland", "client1.irc.net");
    simulateClientSession(server, 5, "bob", "bob", "Bob Builder", "client2.irc.net");
    simulateClientSession(server, 6, "charlie", "charlie", "Charlie Brown", "client3.irc.net");
    
    std::cout << "\nServer Status after registrations:" << std::endl;
    server.listAllClients();
    
    // Test 2: Channel creation and joining
    printSeparator("TEST 2: Channel Operations");
    std::cout << ">> alice creates and joins #general" << std::endl;
    server.processMessage(4, "JOIN #general");
    
    std::cout << "\n>> bob joins #general" << std::endl;
    server.processMessage(5, "JOIN #general");
    
    std::cout << "\n>> charlie joins #general" << std::endl;
    server.processMessage(6, "JOIN #general");
    
    std::cout << "\n>> alice creates #private channel" << std::endl;
    server.processMessage(4, "JOIN #private");
    
    server.listAllChannels();
    
    // Test 3: Channel communication
    printSeparator("TEST 3: Channel Communication");
    std::cout << ">> alice sends welcome message to #general" << std::endl;
    server.processMessage(4, "PRIVMSG #general :Hello everyone! Welcome to our IRC server!");
    
    std::cout << "\n>> bob responds in #general" << std::endl;
    server.processMessage(5, "PRIVMSG #general :Hey alice! Thanks for setting this up!");
    
    std::cout << "\n>> charlie joins the conversation" << std::endl;
    server.processMessage(6, "PRIVMSG #general :Hi all! Great to be here :)");
    
    // Test 4: Private messaging
    printSeparator("TEST 4: Private Messages");
    std::cout << ">> alice sends private message to bob" << std::endl;
    server.processMessage(4, "PRIVMSG bob :Can you help me set up the channel modes?");
    
    std::cout << "\n>> bob replies privately to alice" << std::endl;
    server.processMessage(5, "PRIVMSG alice :Sure! I'll show you how to use MODE commands.");
    
    // Test 5: Channel management
    printSeparator("TEST 5: Channel Management & Modes");
    std::cout << ">> alice sets topic for #general" << std::endl;
    server.processMessage(4, "TOPIC #general :Welcome to ft_irc - A C++ IRC Server Implementation");
    
    std::cout << "\n>> alice makes bob an operator" << std::endl;
    server.processMessage(4, "MODE #general +o bob");
    
    std::cout << "\n>> alice enables topic protection" << std::endl;
    server.processMessage(4, "MODE #general +t");
    
    std::cout << "\n>> bob tries to set channel to invite-only" << std::endl;
    server.processMessage(5, "MODE #general +i");
    
    server.printChannelInfo("#general");
    
    // Test 6: Advanced channel features
    printSeparator("TEST 6: Advanced Features");
    std::cout << ">> alice invites charlie to #private" << std::endl;
    server.processMessage(4, "INVITE charlie #private");
    
    std::cout << "\n>> charlie joins #private after invitation" << std::endl;
    server.processMessage(6, "JOIN #private");
    
    std::cout << "\n>> alice sets a channel key (password)" << std::endl;
    server.processMessage(4, "MODE #private +k secretkey");
    
    server.printChannelInfo("#private");
    
    // Test 7: Client departure
    printSeparator("TEST 7: Client Departure");
    std::cout << ">> charlie leaves #general with a message" << std::endl;
    server.processMessage(6, "PART #general :Going to focus on the private channel");
    
    std::cout << "\n>> bob checks who's still in #general" << std::endl;
    server.processMessage(5, "WHO #general");
    
    std::cout << "\n>> charlie quits the server" << std::endl;
    server.processMessage(6, "QUIT :Thanks for the test session!");
    
    std::cout << "\nFinal server state:" << std::endl;
    server.listAllClients();
    server.listAllChannels();
}

void testErrorHandling() {
    printSeparator("ERROR HANDLING & EDGE CASES TEST");
    
    Server server("ft_irc_test", "secret123");
    
    // Test unregistered client commands
    std::cout << ">> Testing commands from unregistered client..." << std::endl;
    server.addClient(10, "unregistered.client.com");
    server.processMessage(10, "JOIN #test");  // Should fail - not registered
    server.processMessage(10, "PRIVMSG #test :Hello");  // Should fail
    
    // Test wrong password
    std::cout << "\n>> Testing wrong password..." << std::endl;
    server.addClient(11, "badauth.client.com");
    server.processMessage(11, "PASS wrongpassword");
    server.processMessage(11, "NICK testuser");
    server.processMessage(11, "USER test 0 * :Test User");
    
    // Test duplicate nicknames
    std::cout << "\n>> Testing duplicate nicknames..." << std::endl;
    server.addClient(12, "duplicate.client.com");
    server.processMessage(12, "PASS secret123");
    server.processMessage(12, "NICK alice");  // Should fail if alice already exists
    
    // Test invalid channel names
    std::cout << "\n>> Testing invalid channel operations..." << std::endl;
    server.addClient(13, "valid.client.com");
    server.processMessage(13, "PASS secret123");
    server.processMessage(13, "NICK validuser");
    server.processMessage(13, "USER valid 0 * :Valid User");
    server.processMessage(13, "JOIN invalidchannel");  // No # prefix
    server.processMessage(13, "JOIN #");  // Empty channel name
    
    server.listAllClients();
}

void runAutomatedTests() {
    std::cout << "\nAvailable automated tests:" << std::endl;
    std::cout << " 1. Basic IRC Flow Test" << std::endl;
    std::cout << " 2. Error Handling Test" << std::endl;
    std::cout << " 3. Performance Test" << std::endl;
    std::cout << " 4. Run All Tests" << std::endl;
    std::cout << " 0. Back to main menu" << std::endl;
    
    int choice;
    std::cout << "\nChoice: ";
    std::cin >> choice;
    std::cin.ignore();
    
    switch (choice) {
        case 1: testBasicIRCFlow(); break;
        case 2: testErrorHandling(); break;
        case 3: testPerformance(); break;
        case 4: 
            testBasicIRCFlow();
            testErrorHandling();
            testPerformance();
            break;
        default: return;
    }
    
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

void testPerformance() {
    printSeparator("PERFORMANCE & STRESS TEST");
    
    Server server("stress_test", "load123");
    
    std::cout << ">> Creating multiple clients and channels..." << std::endl;
    
    // Create 10 clients
    for (int i = 20; i < 30; i++) {
        std::ostringstream nick, user, real, host;
        nick << "user" << i;
        user << "user" << i;
        real << "Test User " << i;
        host << "client" << i << ".test.com";
        
        server.addClient(i, host.str());
        server.processMessage(i, "PASS load123");
        server.processMessage(i, "NICK " + nick.str());
        server.processMessage(i, "USER " + user.str() + " 0 * :" + real.str());
    }
    
    // All clients join a common channel
    std::cout << ">> All clients joining #lobby..." << std::endl;
    for (int i = 20; i < 30; i++) {
        server.processMessage(i, "JOIN #lobby");
    }
    
    // Simulate active chat
    std::cout << ">> Simulating active chat..." << std::endl;
    for (int i = 20; i < 25; i++) {
        std::ostringstream msg;
        msg << "PRIVMSG #lobby :This is message from user" << i;
        server.processMessage(i, msg.str());
    }
    
    server.printChannelInfo("#lobby");
    
    std::cout << ">> Stress test complete - " << 10 << " clients, 1 active channel" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // Check for automated test mode
        if (argc > 1) {
            std::string test = argv[1];
            if (test == "basic" || test == "b") {
                testBasicIRCFlow();
                return 0;
            } else if (test == "errors" || test == "e") {
                testErrorHandling();
                return 0;
            } else if (test == "performance" || test == "p") {
                testPerformance();
                return 0;
            } else if (test == "all" || test == "a") {
                testBasicIRCFlow();
                testErrorHandling();
                testPerformance();
                return 0;
            } else if (test == "help" || test == "-h" || test == "--help") {
                std::cout << "ft_irc Interactive Test Console" << std::endl;
                std::cout << "===============================" << std::endl;
                std::cout << "\nUsage: " << argv[0] << " [option]" << std::endl;
                std::cout << "\nOptions:" << std::endl;
                std::cout << "  (no args)      - Start interactive mode" << std::endl;
                std::cout << "  basic (b)      - Run basic IRC functionality test" << std::endl;
                std::cout << "  errors (e)     - Run error handling test" << std::endl;
                std::cout << "  performance (p) - Run performance test" << std::endl;
                std::cout << "  all (a)        - Run all automated tests" << std::endl;
                std::cout << "  help (-h)      - Show this help message" << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown option: " << test << std::endl;
                std::cerr << "Use '" << argv[0] << " help' for usage information." << std::endl;
                return 1;
            }
        }
        
        // Interactive mode
        printHeader();
        std::cout << "\nWelcome to the ft_irc Interactive Test Console!" << std::endl;
        std::cout << "Here you can manually test your IRC server functionality." << std::endl;
        
        int choice;
        do {
            showMenu();
            std::cout << "Enter your choice: ";
            std::cin >> choice;
            std::cin.ignore(); // Clear the newline
            
            switch (choice) {
                case 1:
                    serverManagementMenu();
                    break;
                case 2:
                    clientManagementMenu();
                    break;
                case 3:
                    channelOperationsMenu();
                    break;
                case 4:
                    sendCommandMenu();
                    break;
                case 5:
                    viewServerState();
                    break;
                case 6:
                    runAutomatedTests();
                    break;
                case 7:
                    printHeader();
                    break;
                case 0:
                    std::cout << "\n👋 Goodbye! Thanks for testing ft_irc!" << std::endl;
                    break;
                default:
                    std::cout << "❌ Invalid choice. Please try again." << std::endl;
                    break;
            }
            
            if (choice != 0 && choice != 7) {
                std::cout << "\nPress Enter to continue...";
                std::cin.get();
            }
            
        } while (choice != 0);
        
        // Cleanup
        if (g_server) {
            delete g_server;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(50, 'X') << std::endl;
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        std::cerr << std::string(50, 'X') << std::endl;
        if (g_server) delete g_server;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(50, 'X') << std::endl;
        std::cerr << "UNKNOWN FATAL ERROR OCCURRED" << std::endl;
        std::cerr << std::string(50, 'X') << std::endl;
        if (g_server) delete g_server;
        return 1;
    }
}