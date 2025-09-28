#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>

class Message {
public:
    Message(const std::string &raw);
    ~Message();

    const std::string &getPrefix() const;
    const std::string &getCommand() const;
    const std::vector<std::string> &getParams() const;

private:
    std::string _prefix;
    std::string _command;
    std::vector<std::string> _params;

    void parse(const std::string &raw);
};

#endif // MESSAGE_HPP
