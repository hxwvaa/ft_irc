#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <map>

class Client;

class Channel {
public:
    Channel();
    Channel(const std::string &name);
    ~Channel();

    const std::string &getName() const;
    const std::vector<Client*> &getClients() const;
    void addClient(Client *client);
    void removeClient(Client *client);
    bool hasClient(Client *client) const;
    void broadcast(const std::string &message, Client *exclude = 0);

private:
    std::string _name;
    std::vector<Client *> _clients;
    std::map<std::string, std::string> _modes;
};

#endif // CHANNEL_HPP
