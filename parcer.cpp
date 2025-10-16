#include "parcer.hpp"
#include "Server.hpp"
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <iostream>

std::vector<std::string> parseMessage(const std::string& message) {
    std::vector<std::string> tokens;
    
    if (message.empty()) return tokens;
    
    // Find the start of the message (skip any leading whitespace)
    size_t pos = 0;
    while (pos < message.length() && (message[pos] == ' ' || message[pos] == '\t')) {
        pos++;
    }
    // Parse the message according to IRC RFC 2812
    // Format: [':' prefix SPACE] command [SPACE params] [SPACE ':' trailing]

    // Skip prefix if present (starts with ':')
    if (pos < message.length() && message[pos] == ':') {
        // Skip to next space (end of prefix)
        while (pos < message.length() && message[pos] != ' ') {
            pos++;
        }
        // Skip spaces after prefix
        while (pos < message.length() && message[pos] == ' ') {
            pos++;
        }
    }
    // Parse command and parameters
    while (pos < message.length()) {
        if (message[pos] == ' ') {
            // Skip multiple spaces
            while (pos < message.length() && message[pos] == ' ') {
                pos++;
            }
            continue;
        }
        
        if (message[pos] == ':') {
            // Trailing parameter - everything after ':' until end of line
            pos++; // Skip the ':'
            std::string trailing = message.substr(pos);
            // Remove trailing \r\n if present
            size_t end = trailing.find_last_not_of("\r\n");
            if (end != std::string::npos) {
                trailing = trailing.substr(0, end + 1);
            }
            if (!trailing.empty()) {
                tokens.push_back(trailing);
            }
            break;
        } else {
            // Regular parameter - until next space or ':'
            size_t start = pos;
            while (pos < message.length() && message[pos] != ' ' && message[pos] != ':') {
                pos++;
            }
            std::string param = message.substr(start, pos - start);
            // Remove trailing \r\n if it's the last parameter
            size_t end = param.find_last_not_of("\r\n");
            if (end != std::string::npos) {
                param = param.substr(0, end + 1);
            }
            if (!param.empty()) {
                tokens.push_back(param);
            }
        }
    }
    
    return tokens;
}
// Helper function: Split string by commas
std::vector<std::string> splitByComma(const std::string& str) {
    std::vector<std::string> result;
    std::string current;
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == ',') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += str[i];
        }
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}

// Helper function: Convert string to int safely
bool stringToInt(const std::string& str, int& result) {
    if (str.empty()) return false;
    char* end;
    long val = std::strtol(str.c_str(), &end, 10);
    if (*end != '\0') return false;
    result = static_cast<int>(val);
    return true;
}

// Helper function: Check if nickname is valid
bool isValidNickname(const std::string& nick) {
    if (nick.empty() || nick.length() > 9) return false;
    if (!std::isalpha(nick[0])) return false;
    
    for (size_t i = 1; i < nick.length(); ++i) {
        char c = nick[i];
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '[' && c != ']' && c != '{' && c != '}' && c != '\\' && c != '|') {
            return false;
        }
    }
    return true;
}

