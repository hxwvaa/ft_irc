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
    std::map<std::string, std::set<int> > _channels; // channel name -> set of client fds

    void setup();
    void handleNewConnection();
    void handleClientData(int fd);
    void removeClient(int fd);
    void processMessage(int fd, const std::string& message);
    void sendReply(int fd, const std::string& reply);
    void broadcastToChannel(const std::string& channel, const std::string& message, int exclude_fd = -1);
    
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
    
    std::vector<std::string> parseMessage(const std::string& message);
    std::string extractCommand(const std::string& message);
    bool isValidNickname(const std::string& nick);
    static void signalHandler(int signum);
};

#endif // SERVER_HPP
