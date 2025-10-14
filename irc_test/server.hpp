// server.hpp
#ifndef SERVER_HPP
#define SERVER_HPP

#include "parcer.hpp"
#include "client.hpp"
#include "channel.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

class Server : public ServerAdapter {
private:
    std::string _serverName;
    std::string _serverPassword;
    
    // Static empty strings for returning references when no client/channel exists
    static const std::string _emptyString;
    
    // Client management
    std::map<int, Client*> _clients;  // fd -> Client*
    std::map<std::string, int> _nickToFd;  // nickname -> fd
    
    // Channel management
    std::map<std::string, Channel*> _channels;
    
    // Invite tracking (channel -> set of fds)
    std::map<std::string, std::set<int> > _invites;

public:
    Server(const std::string& name, const std::string& password);
    ~Server();
    
    // Client management methods
    void addClient(int fd, const std::string& hostname = "localhost");
    void removeClient(int fd);
    void processMessage(int fd, const std::string& message);
    
    // Test helper methods
    void printClientInfo(int fd) const;
    void printChannelInfo(const std::string& channelName) const;
    void listAllClients() const;
    void listAllChannels() const;
    
    // ServerAdapter interface implementation - CHANGED RETURN TYPES
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
    bool isOper(int fd) const;
    void setOper(int fd, bool v);
    
    bool isValidNick(const std::string& nick) const;
    int fdByNick(const std::string& nick) const;
    
    bool channelExists(const std::string& name) const;
    void createChannel(const std::string& name, int creatorFd);
    bool isInChannel(int fd, const std::string& name) const;
    void joinChannel(int fd, const std::string& name);
    void partChannel(int fd, const std::string& name, const std::string& reason);
    bool isChanOper(int fd, const std::string& name) const;
    void setChanOper(int fd, const std::string& name, int targetFd, bool v);
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
    int limit(const std::string& name) const;
    void set_limit(const std::string& name, int n);
    bool isInvited(const std::string& name, int fd) const;
    void addInvite(const std::string& name, int fd);
    void removeInvite(const std::string& name, int fd);
    
    void sendToFd(int fd, const std::string& msg);
    void sendNumeric(int fd, int code, const std::string& msgSuffix);
    void broadcastToChannel(const std::string& name, const std::string& msg, int exceptFd);
    void noticeToFd(int fd, const std::string& msg);
    
    std::vector<std::string> listChannels() const;
    size_t channelSize(const std::string& name) const;

private:
    // Helper methods
    Client* getClient(int fd) const;
    Client* getClientByNick(const std::string& nick) const;
    Channel* getChannel(const std::string& name) const;
    Channel* getOrCreateChannel(const std::string& name, int creatorFd);
};

#endif