#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include "parcer.hpp"

struct ChannelInfo {
    std::set<int> members;          // Client file descriptors
    std::set<int> operators;        // Operator file descriptors
    std::set<int> invited;          // Invited client file descriptors (for +i mode)
    std::string key;                // Channel password
    size_t userLimit;               // 0 means no limit
    bool inviteOnly;                // +i mode
    bool topicRestricted;           // +t mode
    std::string topic;              // Channel topic
    
    ChannelInfo() : userLimit(0), inviteOnly(false), topicRestricted(true) {}
    
    std::string getModeString() const;
};

struct ClientInfo {
    int fd;
    std::string buffer;
    std::string nickname;
    std::string username;
    std::string realname;
    std::string hostname;
    bool authenticated;
    bool registered;
    std::set<std::string> channels;
    
    ClientInfo() : fd(-1), authenticated(false), registered(false) {}
    ClientInfo(int socket_fd) : fd(socket_fd), authenticated(false), registered(false) {}
};

class Server {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();
    
    // Public helper functions for command handlers
    std::string getPassword() const { return _password; }
    ClientInfo& getClient(int fd);
    ChannelInfo& getChannel(const std::string& name);
    std::map<int, ClientInfo>& getClients() { return _clients; }
    std::map<std::string, ChannelInfo>& getChannels() { return _channels; }
    
    void sendReply(int fd, const std::string& reply);
    void broadcastToChannel(const std::string& channel, const std::string& message, int exclude_fd = -1);
    std::string formatServerReply(int fd, const std::string& numericAndParams) const;
    std::string formatUserMessage(int fd, const std::string& command) const;
    
    bool isChannelOperator(const std::string& channel, int fd);
    bool isClientInChannel(const std::string& channel, int fd);
    int getClientFdByNick(const std::string& nickname);
    void removeClient(int fd);
    
    static const std::string SERVER_NAME;

private:
    int _port;
    std::string _password;
    int _sockfd;
    std::vector<struct pollfd> _fds;
    std::map<int, ClientInfo> _clients;
    std::map<std::string, ChannelInfo> _channels; // channel name -> channel info

    void setup();
    void handleNewConnection();
    void handleClientData(int fd);
    void processMessage(int fd, const std::string& message);
    
    static void signalHandler(int signum);
};

#endif // SERVER_HPP