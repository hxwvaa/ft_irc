#ifndef PARCER_HPP
#define PARCER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstddef>
#include <climits>

class Server;

// Adapter interface your Server implements (no extra files needed).
struct ServerAdapter {
    virtual ~ServerAdapter() {}

    // --- server identity/config ---
    virtual const std::string& serverName() const = 0;
    virtual const std::string& serverPassword() const = 0;

    // --- client state ---
    virtual bool hasClient(int fd) const = 0;
    virtual bool isRegistered(int fd) const = 0;
    virtual void setRegistered(int fd, bool v) = 0;
    virtual const std::string& getNick(int fd) const = 0;
    virtual void setNick(int fd, const std::string& nick) = 0;
    virtual const std::string& getUser(int fd) const = 0;
    virtual void setUser(int fd, const std::string& user) = 0;
    virtual const std::string& getRealname(int fd) const = 0;
    virtual void setRealname(int fd, const std::string& rn) = 0;
    virtual bool checkPasswordGiven(int fd) const = 0;
    virtual void setPasswordGiven(int fd, bool v) = 0;
    virtual bool isOper(int fd) const = 0;
    virtual void setOper(int fd, bool v) = 0;

    // --- nickname checks ---
    virtual bool isValidNick(const std::string& nick) const = 0;
    virtual int  fdByNick(const std::string& nick) const = 0; // -1 if none

    // --- channel state ---
    virtual bool channelExists(const std::string& name) const = 0;
    virtual void createChannel(const std::string& name, int creatorFd) = 0;
    virtual bool isInChannel(int fd, const std::string& name) const = 0;
    virtual void joinChannel(int fd, const std::string& name) = 0;
    virtual void partChannel(int fd, const std::string& name, const std::string& reason) = 0;
    virtual bool isChanOper(int fd, const std::string& name) const = 0;
    virtual void setChanOper(int fd, const std::string& name, int targetFd, bool v) = 0;
    virtual const std::set<int>& channelMembers(const std::string& name) const = 0;

    // --- channel topic/modes ---
    virtual const std::string& getTopic(const std::string& name) const = 0;
    virtual void setTopic(const std::string& name, const std::string& topic) = 0;
    virtual bool mode_i(const std::string& name) const = 0; // invite-only
    virtual void set_mode_i(const std::string& name, bool v) = 0;
    virtual bool mode_t(const std::string& name) const = 0; // topic by ops only
    virtual void set_mode_t(const std::string& name, bool v) = 0;
    virtual bool has_key(const std::string& name) const = 0;
    virtual const std::string& key(const std::string& name) const = 0;
    virtual void set_key(const std::string& name, const std::string& k) = 0;
    virtual void unset_key(const std::string& name) = 0;
    virtual int  limit(const std::string& name) const = 0; // 0 = unlimited
    virtual void set_limit(const std::string& name, int n) = 0;
    virtual bool isInvited(const std::string& name, int fd) const = 0;
    virtual void addInvite(const std::string& name, int fd) = 0;
    virtual void removeInvite(const std::string& name, int fd) = 0;

    // --- messaging ---
    virtual void sendToFd(int fd, const std::string& msg) = 0;
    virtual void sendNumeric(int fd, int code, const std::string& msgSuffix) = 0;
    virtual void broadcastToChannel(const std::string& name, const std::string& msg, int exceptFd) = 0;
    virtual void noticeToFd(int fd, const std::string& msg) = 0;

    // --- queries ---
    virtual std::vector<std::string> listChannels() const = 0;
    virtual size_t channelSize(const std::string& name) const = 0;
};

struct IRCCommand {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
};

class Parcer {
public:
    explicit Parcer(ServerAdapter* adapter): _A(adapter) {}

	static bool isValidNicknameLocal(const std::string& s);
    static bool isValidChannelNameLocal(const std::string& s);

    IRCCommand parse(const std::string& line) const;
    void handle(int fd, const IRCCommand& cmd);

private:
    ServerAdapter* _A;

    static std::string trim(const std::string& s);
    static void to_upper(std::string& s);
    static bool isChannel(const std::string& name);
    void tryFinishRegistration(int fd);
    static bool safeAtoi(const std::string& s, int& out);

    // command handlers
    void do_PASS(int fd, const std::vector<std::string>& P);
    void do_NICK(int fd, const std::vector<std::string>& P);
    void do_USER(int fd, const std::vector<std::string>& P);
    void do_PING(int fd, const std::vector<std::string>& P);
    void do_PONG(int fd, const std::vector<std::string>& P);
    void do_JOIN(int fd, const std::vector<std::string>& P);
    void do_PART(int fd, const std::vector<std::string>& P);
    void do_PRIVMSG(int fd, const std::vector<std::string>& P, bool isNotice);
    void do_TOPIC(int fd, const std::vector<std::string>& P);
    void do_INVITE(int fd, const std::vector<std::string>& P);
    void do_KICK(int fd, const std::vector<std::string>& P);
    void do_MODE(int fd, const std::vector<std::string>& P);
    void do_LIST(int fd, const std::vector<std::string>& P);
    void do_WHO(int fd, const std::vector<std::string>& P);
    void do_QUIT(int fd, const std::vector<std::string>& P);
};

#endif // PARCER_HPP