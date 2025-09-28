#include "Message.hpp"
#include <iostream>
#include <sstream>

Message::Message(const std::string &raw) {
    parse(raw);
}

Message::~Message() {}

void Message::parse(const std::string &raw) {
    std::string temp = raw;
    if (temp.empty()) return;

    // Prefix
    if (temp[0] == ':') {
        size_t prefix_end = temp.find(' ');
        if (prefix_end != std::string::npos) {
            _prefix = temp.substr(1, prefix_end - 1);
            temp = temp.substr(prefix_end + 1);
        }
    }

    // Command
    size_t command_end = temp.find(' ');
    if (command_end != std::string::npos) {
        _command = temp.substr(0, command_end);
        temp = temp.substr(command_end + 1);
    } else {
        _command = temp;
        return;
    }

    // Parameters
    size_t colon_pos = temp.find(':');
    if (colon_pos != std::string::npos) {
        std::string before_colon = temp.substr(0, colon_pos);
        std::string after_colon = temp.substr(colon_pos + 1);
        
        std::stringstream ss(before_colon);
        std::string param;
        while (ss >> param) {
            _params.push_back(param);
        }
        if (!after_colon.empty()) {
            _params.push_back(after_colon);
        }
    } else {
        std::stringstream ss(temp);
        std::string param;
        while (ss >> param) {
            _params.push_back(param);
        }
    }
}

const std::string &Message::getPrefix() const {
    return _prefix;
}

const std::string &Message::getCommand() const {
    return _command;
}

const std::vector<std::string> &Message::getParams() const {
    return _params;
}
