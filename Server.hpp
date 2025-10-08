#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include "parcer.hpp"

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

class Server : public ServerAdapter {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();

//-----Additions form Parsing-----
// === ServerAdapter overrides (declare these in Server.hpp public:) ===
    const std::string& serverName() const;
    const std::string& serverPassword() const;

    bool hasClient(int fd) const;
    bool isRegistered(int fd) const;
    void setRegistered(int fd, bool v);
    const std::string& getNick(int fd) const;
    void setNick(int fd, const std::string& nick);
    const std::string& getUser(int fd) const;
    void setUser(int fd, const std::string& user);
    const std::string& getRealname(int fd) const;
    void setRealname(int fd, const std::string& rn);
    bool checkPasswordGiven(int fd) const;
    void setPasswordGiven(int fd, bool v);
    bool isOper(int fd) const;             // (optional; return false for now)
    void setOper(int fd, bool v);          // (optional; no-op)

    bool isValidNick(const std::string& nick) const;
    int  fdByNick(const std::string& nick) const;

    bool channelExists(const std::string& name) const;
    void createChannel(const std::string& name, int creatorFd);
    bool isInChannel(int fd, const std::string& name) const;
    void joinChannel(int fd, const std::string& name);
    void partChannel(int fd, const std::string& name, const std::string& reason);
    bool isChanOper(int fd, const std::string& name) const;
    void setChanOper(int byFd, const std::string& name, int targetFd, bool v);
    const std::set<int>& channelMembers(const std::string& name) const;

    const std::string& getTopic(const std::string& name) const;
    void setTopic(const std::string& name, const std::string& topic);
    bool mode_i(const std::string& name) const;
    void set_mode_i(const std::string& name, bool v);
    bool mode_t(const std::string& name) const;
    void set_mode_t(const std::string& name, bool v);
    bool has_key(const std::string& name) const;
    const std::string& key(const std::string& name) const;
    void set_key(const std::string& name, const std::string& k);
    void unset_key(const std::string& name);
    int  limit(const std::string& name) const;
    void set_limit(const std::string& name, int n);
    bool isInvited(const std::string& name, int fd) const;
    void addInvite(const std::string& name, int fd);
    void removeInvite(const std::string& name, int fd);

    void sendToFd(int fd, const std::string& msg);
    void sendNumeric(int fd, int code, const std::string& msgSuffix);
    // NOTE: broadcastToChannel(...) already exists with the right signature
    void noticeToFd(int fd, const std::string& msg);

    std::vector<std::string> listChannels() const;
    size_t channelSize(const std::string& name) const;
    //=================================================================

private:
    int _port;
    std::string _password;
    int _sockfd;
    std::vector<struct pollfd> _fds;
    std::map<int, ClientInfo> _clients;
    std::map<std::string, std::set<int> > _channels; // channel name -> set of client fds
//-----Additions from Parsing-----
    std::map<std::string, std::string>        _chanTopic;   // channel -> topic
    std::map<std::string, std::set<int> >     _chanOps;     // channel -> operator fds
    std::map<std::string, std::string>        _chanKey;     // MODE +k key
    std::map<std::string, int>                _chanLimit;   // MODE +l limit (0 = none)
    std::set<std::string>                     _mode_i;      // invite-only (+i)
    std::set<std::string>                     _mode_t;      // topic ops-only (+t)
    std::map<std::string, std::set<int> >     _invites;     // channel -> invited fds

    Parcer _parcer;

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
    static bool isValidNickname(const std::string& nick);
    static void signalHandler(int signum);
};

#endif
