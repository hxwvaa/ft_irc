#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h>

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
    
    std::string getModeString() const {
        std::string modes = "+";
        if (inviteOnly) modes += "i";
        if (topicRestricted) modes += "t";
        if (!key.empty()) modes += "k";
        if (userLimit > 0) modes += "l";
        return modes;
    }
};

class Server {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();

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
    void removeClient(int fd);
    void processMessage(int fd, const std::string& message);
    void sendReply(int fd, const std::string& reply);
    void broadcastToChannel(const std::string& channel, const std::string& message, int exclude_fd = -1);
    
    // Helper functions for MODE
    bool isChannelOperator(const std::string& channel, int fd);
    bool isClientInChannel(const std::string& channel, int fd);
    int getClientFdByNick(const std::string& nickname);
    bool stringToInt(const std::string& str, int& result);
    
    // IRC command handlers
    void handlePass(int fd, const std::vector<std::string>& params);
    void handleNick(int fd, const std::vector<std::string>& params);
    void handleUser(int fd, const std::vector<std::string>& params);
    void handleJoin(int fd, const std::vector<std::string>& params);
    void handlePrivMsg(int fd, const std::vector<std::string>& params);
    void handleQuit(int fd, const std::vector<std::string>& params);
    void handlePing(int fd, const std::vector<std::string>& params);
    void handlePart(int fd, const std::vector<std::string>& params);
    void handleMode(int fd, const std::vector<std::string>& params);
    void handleWho(int fd, const std::vector<std::string>& params);
    void handleList(int fd, const std::vector<std::string>& params);
    void handleKick(int fd, const std::vector<std::string>& params);
    void handleInvite(int fd, const std::vector<std::string>& params);
    
    std::vector<std::string> parseMessage(const std::string& message);
    bool isValidNickname(const std::string& nick);
    static void signalHandler(int signum);
};

#endif // SERVER_HPP