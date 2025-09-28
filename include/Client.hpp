#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <set>

class Client {
public:
    Client();
    Client(int fd);
    ~Client();

    int getFd() const;
    bool isAuthenticated() const;
    void setAuthenticated(bool authenticated);
    const std::string &getNickname() const;
    void setNickname(const std::string &nickname);
    const std::string &getUsername() const;
    void setUsername(const std::string &username);
    const std::string &getRealname() const;
    void setRealname(const std::string &realname);
    bool isRegistered() const;
    void checkRegistration();

    std::string _buffer;
private:
    int _fd;
    bool _authenticated;
    bool _registered;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    
    friend class Server;
};

#endif // CLIENT_HPP
