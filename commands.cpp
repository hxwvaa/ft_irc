#include "parcer.hpp"
#include "Server.hpp"
#include <sstream>
#include <iostream>

// Command handler implementations

void handlePass(Server* server, int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        server->sendReply(fd, server->formatServerReply(fd, "461 * PASS :Not enough parameters"));
        return;
    }
    
    ClientInfo& client = server->getClient(fd);
    if (params[0] == server->getPassword()) {
        client.authenticated = true;
        // std::cout << "Client " << fd << " authenticated" << std::endl;
    } else {
        server->sendReply(fd, server->formatServerReply(fd, "464 * :Password incorrect"));
    }
}

void handleNick(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    
    // Check if client has authenticated first
    if (!client.authenticated) {
        server->sendReply(fd, server->formatServerReply(fd, "464 * :Password required"));
        return;
    }
    
    if (params.empty()) {
        server->sendReply(fd, server->formatServerReply(fd, "431 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :No nickname given"));
        return;
    }

    std::string new_nick = params[0];
    
    // Validate nickname format
    if (!isValidNickname(new_nick)) {
        server->sendReply(fd, server->formatServerReply(fd, "432 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " " + new_nick + " :Erroneous nickname"));
        return;
    }
    
    // Check if nickname is already in use by another client
    std::map<int, ClientInfo>& clients = server->getClients();
    for (std::map<int, ClientInfo>::iterator it = clients.begin(); 
         it != clients.end(); ++it) {
        if (it->first != fd && it->second.nickname == new_nick) {
            server->sendReply(fd, server->formatServerReply(fd, "433 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " " + new_nick + " :Nickname is already in use"));
            return;
        }
    }
    
    std::string old_nick = client.nickname;
    client.nickname = new_nick;
    
    // If user was already registered, notify channels about nick change
    if (client.registered && !old_nick.empty()) {
        std::string nick_msg = ":" + old_nick + "!" + client.username + "@" + client.hostname + " NICK :" + new_nick + "\r\n";
        for (std::set<std::string>::iterator it = client.channels.begin(); 
             it != client.channels.end(); ++it) {
            server->broadcastToChannel(*it, nick_msg, -1);
        }
        server->sendReply(fd, nick_msg);
    }
    
    // std::cout << "Client " << fd << " set nickname: " << new_nick << std::endl;
    
    // Check if we can complete registration (only if not already registered)
    if (!client.registered && client.authenticated && !client.nickname.empty() && !client.username.empty()) {
        client.registered = true;
        server->sendReply(fd, server->formatServerReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname));
        server->sendReply(fd, server->formatServerReply(fd, "002 " + client.nickname + " :Your host is " + server->SERVER_NAME + ", running version 1.0"));
        server->sendReply(fd, server->formatServerReply(fd, "003 " + client.nickname + " :This server was created today"));
        server->sendReply(fd, server->formatServerReply(fd, "004 " + client.nickname + " " + server->SERVER_NAME + " 1.0 o o"));
        server->sendReply(fd, server->formatServerReply(fd, "375 " + client.nickname + " :- " + server->SERVER_NAME + " Message of the day -"));
        server->sendReply(fd, server->formatServerReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!"));
        server->sendReply(fd, server->formatServerReply(fd, "376 " + client.nickname + " :End of MOTD command"));
        // std::cout << "Client " << fd << " completed registration as " << client.nickname << std::endl;
    }
}

void handleUser(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    
    // Check if client has authenticated first
    if (!client.authenticated) {
        server->sendReply(fd, server->formatServerReply(fd, "464 * :Password required"));
        return;
    }
    
    // Reject USER command if already registered
    if (client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "462 " + client.nickname + " :You may not reregister"));
        return;
    }
    
    if (params.size() < 4) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " USER :Not enough parameters"));
        return;
    }

    client.username = params[0];
    // params[1] is mode (unused), params[2] is unused
    // Set hostname to localhost (in a real server, would use client's actual hostname)
    client.hostname = "localhost";
    client.realname = params[3];
    
    // std::cout << "Client " << fd << " set user info: " << client.username << std::endl;
    
    // Check if we can complete registration (only if not already registered)
    if (!client.registered && client.authenticated && !client.nickname.empty() && !client.username.empty()) {
        client.registered = true;
        server->sendReply(fd, server->formatServerReply(fd, "001 " + client.nickname + " :Welcome to the IRC Network " + client.nickname + "!" + client.username + "@" + client.hostname));
        server->sendReply(fd, server->formatServerReply(fd, "002 " + client.nickname + " :Your host is " + server->SERVER_NAME + ", running version 1.0"));
        server->sendReply(fd, server->formatServerReply(fd, "003 " + client.nickname + " :This server was created today"));
        server->sendReply(fd, server->formatServerReply(fd, "004 " + client.nickname + " " + server->SERVER_NAME + " 1.0 o o"));
        server->sendReply(fd, server->formatServerReply(fd, "375 " + client.nickname + " :- " + server->SERVER_NAME + " Message of the day -"));
        server->sendReply(fd, server->formatServerReply(fd, "372 " + client.nickname + " :- Welcome to our IRC server!"));
        server->sendReply(fd, server->formatServerReply(fd, "376 " + client.nickname + " :End of MOTD command"));
        // std::cout << "Client " << fd << " completed registration as " << client.nickname << std::endl;
    }
}

void handleJoin(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " JOIN :Not enough parameters"));
        return;
    }

    // Parse channels and keys
    std::vector<std::string> channels = splitByComma(params[0]);
    std::vector<std::string> keys;
    
    if (params.size() > 1) {
        keys = splitByComma(params[1]);
    }
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    std::map<int, ClientInfo>& clientMap = server->getClients();
    
    // Join each channel
    for (size_t i = 0; i < channels.size(); ++i) {
        std::string channel = channels[i];
        std::string key = "";
        
        // Get corresponding key if available
        if (i < keys.size()) {
            key = keys[i];
        }
        
        // Ensure channel name starts with #
        if (channel.empty()) continue;
        if (channel[0] != '#') {
            channel = "#" + channel;
        }
        
        // Check if already in channel
        if (client.channels.find(channel) != client.channels.end()) {
            continue; // Already in this channel, skip
        }
        
        // Check if channel exists and has restrictions
        bool channelExists = channelMap.find(channel) != channelMap.end();
        
        if (channelExists) {
            ChannelInfo& chanInfo = channelMap[channel];
            
            // Check invite-only mode
            if (chanInfo.inviteOnly) {
                // Check if user is invited
                if (chanInfo.invited.find(fd) == chanInfo.invited.end()) {
                    server->sendReply(fd, server->formatServerReply(fd, "473 " + client.nickname + " " + channel + " :Cannot join channel (+i)"));
                    continue; // Skip this channel
                }
                // User is invited, remove from invite list after successful join attempt
                chanInfo.invited.erase(fd);
            }
            
            // Check user limit
            if (chanInfo.userLimit > 0 && chanInfo.members.size() >= chanInfo.userLimit) {
                server->sendReply(fd, server->formatServerReply(fd, "471 " + client.nickname + " " + channel + " :Cannot join channel (+l)"));
                continue; // Skip this channel
            }
            
            // Check key (password)
            if (!chanInfo.key.empty()) {
                if (key != chanInfo.key) {
                    server->sendReply(fd, server->formatServerReply(fd, "475 " + client.nickname + " " + channel + " :Cannot join channel (+k)"));
                    continue; // Skip this channel
                }
            }
        }
        
        // Add user to channel
        client.channels.insert(channel);
        channelMap[channel].members.insert(fd);
        
        // If first member, make them operator
        if (channelMap[channel].members.size() == 1) {
            channelMap[channel].operators.insert(fd);
        }
        
        // Then notify all members (including the one who just joined) about the JOIN
        std::string join_msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " JOIN :" + channel + "\r\n";
        server->broadcastToChannel(channel, join_msg, -1); // Send to everyone including the joiner
        
        // Build list of all users in the channel (including the one who just joined)
        std::string names = "";
        for (std::set<int>::iterator it = channelMap[channel].members.begin(); 
             it != channelMap[channel].members.end(); ++it) {
            if (!names.empty()) names += " ";
            // Add @ prefix for operators
            if (channelMap[channel].operators.find(*it) != channelMap[channel].operators.end()) {
                names += "@";
            }
            names += clientMap[*it].nickname;
        }
        
        server->sendReply(fd, server->formatServerReply(fd, "353 " + client.nickname + " = " + channel + " :" + names));
        server->sendReply(fd, server->formatServerReply(fd, "366 " + client.nickname + " " + channel + " :End of NAMES list"));
        
        // std::cout << "Client " << client.nickname << " joined " << channel << std::endl;
    }
}

void handlePrivMsg(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " PRIVMSG :Not enough parameters"));
        return;
    }

    std::string target = params[0];
    std::string message = params[1];
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    std::map<int, ClientInfo>& clientMap = server->getClients();
    
    if (target[0] == '#') {
        // Channel message
        if (channelMap.find(target) == channelMap.end()) {
            server->sendReply(fd, server->formatServerReply(fd, "403 " + client.nickname + " " + target + " :No such channel"));
            return;
        }
        
        // Check if sender is a member of the channel
        if (client.channels.find(target) == client.channels.end()) {
            server->sendReply(fd, server->formatServerReply(fd, "442 " + client.nickname + " " + target + " :You're not on that channel"));
            return;
        }
        
        std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PRIVMSG " + target + " :" + message + "\r\n";
        server->broadcastToChannel(target, msg, fd);
        // std::cout << "Channel " << target << " <" << client.nickname << "> " << message << std::endl;
    } else {
        // Private message to user
        for (std::map<int, ClientInfo>::iterator it = clientMap.begin(); 
             it != clientMap.end(); ++it) {
            if (it->second.nickname == target) {
                std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PRIVMSG " + target + " :" + message + "\r\n";
                server->sendReply(it->first, msg);
                // std::cout << "Private <" << client.nickname << " -> " << target << "> " << message << std::endl;
                return;
            }
        }
        server->sendReply(fd, server->formatServerReply(fd, "401 " + client.nickname + " " + target + " :No such nick"));
    }
}

void handleQuit(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    std::string quit_msg = "Client Quit";
    if (!params.empty()) {
        quit_msg = params[0];
    }
    
    std::cout << "Client " << client.nickname << " quit: " << quit_msg << std::endl;
    server->removeClient(fd);
}

void handlePing(Server* server, int fd, const std::vector<std::string>& params) {
    if (params.empty()) {
        ClientInfo& client = server->getClient(fd);
        server->sendReply(fd, server->formatServerReply(fd, "409 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :No origin specified"));
        return;
    }
    
    // Respond with PONG
    server->sendReply(fd, ":" + server->SERVER_NAME + " PONG " + server->SERVER_NAME + " :" + params[0] + "\r\n");
}

void handlePart(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " PART :Not enough parameters"));
        return;
    }

    std::string channel = params[0];
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    if (client.channels.find(channel) == client.channels.end()) {
        server->sendReply(fd, server->formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    std::string part_msg = "Leaving";
    if (params.size() > 1) {
        part_msg = params[1];
    }
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    
    client.channels.erase(channel);
    channelMap[channel].members.erase(fd);
    channelMap[channel].operators.erase(fd);
    
    std::string msg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + " PART " + channel + " :" + part_msg + "\r\n";
    server->broadcastToChannel(channel, msg, -1); // Send to all including the one leaving
    
    if (channelMap[channel].members.empty()) {
        channelMap.erase(channel);
    }
    
    // std::cout << "Client " << client.nickname << " left " << channel << std::endl;
}

void handleMode(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.empty()) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " MODE :Not enough parameters"));
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#') {
        // Channel mode
        std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
        std::map<std::string, ChannelInfo>::iterator chanIt = channelMap.find(target);
        if (chanIt == channelMap.end()) {
            server->sendReply(fd, server->formatServerReply(fd, "403 " + client.nickname + " " + target + " :No such channel"));
            return;
        }
        
        ChannelInfo& channel = chanIt->second;
        
        // Check if user is in the channel
        if (!server->isClientInChannel(target, fd)) {
            server->sendReply(fd, server->formatServerReply(fd, "442 " + client.nickname + " " + target + " :You're not on that channel"));
            return;
        }
        
        // If no mode string provided, just show current modes
        if (params.size() == 1) {
            std::string modeStr = channel.getModeString();
            std::string response = "324 " + client.nickname + " " + target + " " + modeStr;
            
            // Add key if present
            if (!channel.key.empty()) {
                response += " " + channel.key;
            }
            // Add limit if present
            if (channel.userLimit > 0) {
                std::ostringstream oss;
                oss << channel.userLimit;
                response += " " + oss.str();
            }
            server->sendReply(fd, server->formatServerReply(fd, response));
            return;
        }
        
        // First check if mode string contains only supported modes
        std::string modeStr = params[1];
        bool hasSupportedModes = false;
        for (size_t i = 0; i < modeStr.length(); ++i) {
            char mode = modeStr[i];
            if (mode == 'i' || mode == 't' || mode == 'k' || mode == 'l' || mode == 'o') {
                hasSupportedModes = true;
                break;
            }
        }
        
        // If no supported modes found, silently ignore
        if (!hasSupportedModes) {
            return;
        }
        
        // Check if user is channel operator (only for supported modes)
        if (!server->isChannelOperator(target, fd)) {
            server->sendReply(fd, server->formatServerReply(fd, "482 " + client.nickname + " " + target + " :You're not channel operator"));
            return;
        }
        
        // Parse mode changes
        bool adding = true;
        size_t paramIndex = 2;
        
        std::string modeChanges = "";
        std::string modeParams = "";
        
        for (size_t i = 0; i < modeStr.length(); ++i) {
            char mode = modeStr[i];
            
            if (mode == '+') {
                adding = true;
                if (modeChanges.empty() || modeChanges[modeChanges.length() - 1] != '+') {
                    modeChanges += "+";
                }
            } else if (mode == '-') {
                adding = false;
                if (modeChanges.empty() || modeChanges[modeChanges.length() - 1] != '-') {
                    modeChanges += "-";
                }
            } else if (mode == 'i') {
                // Invite-only mode
                channel.inviteOnly = adding;
                modeChanges += "i";
            } else if (mode == 't') {
                // Topic restriction mode
                channel.topicRestricted = adding;
                modeChanges += "t";
            } else if (mode == 'k') {
                // Channel key (password)
                if (adding && paramIndex < params.size()) {
                    channel.key = params[paramIndex];
                    modeChanges += "k";
                    if (!modeParams.empty()) modeParams += " ";
                    modeParams += params[paramIndex];
                    paramIndex++;
                } else if (!adding) {
                    channel.key = "";
                    modeChanges += "k";
                }
            } else if (mode == 'l') {
                // User limit
                if (adding && paramIndex < params.size()) {
                    int limit;
                    if (stringToInt(params[paramIndex], limit) && limit > 0) {
                        channel.userLimit = static_cast<size_t>(limit);
                        modeChanges += "l";
                        if (!modeParams.empty()) modeParams += " ";
                        modeParams += params[paramIndex];
                        paramIndex++;
                    }
                } else if (!adding) {
                    channel.userLimit = 0;
                    modeChanges += "l";
                }
            } else if (mode == 'o') {
                // Operator privilege
                if (paramIndex < params.size()) {
                    std::string targetNick = params[paramIndex];
                    int targetFd = server->getClientFdByNick(targetNick);
                    
                    if (targetFd != -1 && server->isClientInChannel(target, targetFd)) {
                        if (adding) {
                            channel.operators.insert(targetFd);
                        } else {
                            channel.operators.erase(targetFd);
                        }
                        modeChanges += "o";
                        if (!modeParams.empty()) modeParams += " ";
                        modeParams += targetNick;
                    }
                    paramIndex++;
                }
            }
            // Silently ignore any other mode characters (like 'b', 'v', 'n', etc.)
        }
        
        // Broadcast mode change to channel
        if (!modeChanges.empty() && modeChanges != "+" && modeChanges != "-") {
            std::string modeMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                                " MODE " + target + " " + modeChanges;
            if (!modeParams.empty()) {
                modeMsg += " " + modeParams;
            }
            modeMsg += "\r\n";
            server->broadcastToChannel(target, modeMsg, -1);
        }
    } else {
        // User mode - we don't support user modes, just send empty mode string
        // server->sendReply(fd, "221 " + client.nickname + " +\r\n");
    }
}

void handleWho(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    if (params.empty()) {
        server->sendReply(fd, "315 " + client.nickname + " * :End of WHO list\r\n");
        return;
    }
    
    std::string target = params[0];
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    std::map<int, ClientInfo>& clientMap = server->getClients();
    
    if (target[0] == '#') {
        // Channel WHO
        std::map<std::string, ChannelInfo>::iterator it = channelMap.find(target);
        if (it != channelMap.end()) {
            for (std::set<int>::iterator client_it = it->second.members.begin(); 
                 client_it != it->second.members.end(); ++client_it) {
                ClientInfo& target_client = clientMap[*client_it];
                server->sendReply(fd, "352 " + client.nickname + " " + target + " " + 
                         target_client.username + " " + target_client.hostname + 
                         " ircserver " + target_client.nickname + " H :0 " + 
                         target_client.realname + "\r\n");
            }
        }
        server->sendReply(fd, "315 " + client.nickname + " " + target + " :End of WHO list\r\n");
    } else {
        server->sendReply(fd, "315 " + client.nickname + " " + target + " :End of WHO list\r\n");
    }
}

void handleList(Server* server, int fd, const std::vector<std::string>& params) {
    (void)params; // Unused parameter
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, "451 :You have not registered\r\n");
        return;
    }
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    
    server->sendReply(fd, "321 " + client.nickname + " Channel :Users  Name\r\n");
    
    for (std::map<std::string, ChannelInfo>::iterator it = channelMap.begin(); 
         it != channelMap.end(); ++it) {
        std::ostringstream oss;
        oss << it->second.members.size();
        server->sendReply(fd, "322 " + client.nickname + " " + it->first + " " + 
                 oss.str() + " :No topic\r\n");
    }
    
    server->sendReply(fd, "323 " + client.nickname + " :End of LIST\r\n");
}

void handleKick(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " KICK :Not enough parameters"));
        return;
    }
    
    std::string channel = params[0];
    std::string targetNick = params[1];
    std::string reason = "Kicked";
    if (params.size() > 2) {
        reason = params[2];
    }
    
    // Ensure channel name starts with #
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    
    // Check if channel exists
    std::map<std::string, ChannelInfo>::iterator chanIt = channelMap.find(channel);
    if (chanIt == channelMap.end()) {
        server->sendReply(fd, server->formatServerReply(fd, "403 " + client.nickname + " " + channel + " :No such channel"));
        return;
    }
    
    ChannelInfo& chanInfo = chanIt->second;
    
    // Check if kicker is in the channel
    if (!server->isClientInChannel(channel, fd)) {
        server->sendReply(fd, server->formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    // Check if kicker is channel operator
    if (!server->isChannelOperator(channel, fd)) {
        server->sendReply(fd, server->formatServerReply(fd, "482 " + client.nickname + " " + channel + " :You're not channel operator"));
        return;
    }
    
    // Find target user
    int targetFd = server->getClientFdByNick(targetNick);
    if (targetFd == -1) {
        server->sendReply(fd, server->formatServerReply(fd, "401 " + client.nickname + " " + targetNick + " :No such nick/channel"));
        return;
    }
    
    // Check if target is in the channel
    if (!server->isClientInChannel(channel, targetFd)) {
        server->sendReply(fd, server->formatServerReply(fd, "441 " + client.nickname + " " + targetNick + " " + channel + " :They aren't on that channel"));
        return;
    }
    
    std::map<int, ClientInfo>& clientMap = server->getClients();
    
    // Get target client info
    ClientInfo& targetClient = clientMap[targetFd];
    
    // Broadcast KICK message to all channel members (including the kicked user)
    std::string kickMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                         " KICK " + channel + " " + targetNick + " :" + reason + "\r\n";
    server->broadcastToChannel(channel, kickMsg, -1);
    
    // Remove user from channel
    targetClient.channels.erase(channel);
    chanInfo.members.erase(targetFd);
    chanInfo.operators.erase(targetFd);
    
    // If channel is empty, delete it
    if (chanInfo.members.empty()) {
        channelMap.erase(channel);
    }
}

void handleInvite(Server* server, int fd, const std::vector<std::string>& params) {
    ClientInfo& client = server->getClient(fd);
    if (!client.registered) {
        server->sendReply(fd, server->formatServerReply(fd, "451 " + (client.nickname.empty() ? std::string("*") : client.nickname) + " :You have not registered"));
        return;
    }
    
    if (params.size() < 2) {
        server->sendReply(fd, server->formatServerReply(fd, "461 " + client.nickname + " INVITE :Not enough parameters"));
        return;
    }
    
    std::string targetNick = params[0];
    std::string channel = params[1];
    
    // Ensure channel name starts with #
    if (channel[0] != '#') {
        channel = "#" + channel;
    }
    
    std::map<std::string, ChannelInfo>& channelMap = server->getChannels();
    
    // Check if channel exists
    std::map<std::string, ChannelInfo>::iterator chanIt = channelMap.find(channel);
    if (chanIt == channelMap.end()) {
        server->sendReply(fd, server->formatServerReply(fd, "403 " + client.nickname + " " + channel + " :No such channel"));
        return;
    }
    
    ChannelInfo& chanInfo = chanIt->second;
    
    // Check if inviter is in the channel
    if (!server->isClientInChannel(channel, fd)) {
        server->sendReply(fd, server->formatServerReply(fd, "442 " + client.nickname + " " + channel + " :You're not on that channel"));
        return;
    }
    
    // If channel is invite-only, only operators can invite
    if (chanInfo.inviteOnly && !server->isChannelOperator(channel, fd)) {
        server->sendReply(fd, server->formatServerReply(fd, "482 " + client.nickname + " " + channel + " :You're not channel operator"));
        return;
    }
    
    // Find target user
    int targetFd = server->getClientFdByNick(targetNick);
    if (targetFd == -1) {
        server->sendReply(fd, server->formatServerReply(fd, "401 " + client.nickname + " " + targetNick + " :No such nick/channel"));
        return;
    }
    
    // Check if target is already in the channel
    if (server->isClientInChannel(channel, targetFd)) {
        server->sendReply(fd, server->formatServerReply(fd, "443 " + client.nickname + " " + targetNick + " " + channel + " :is already on channel"));
        return;
    }
    
    // Add user to invite list
    chanInfo.invited.insert(targetFd);
    
    // Send invite notification to target user
    std::string inviteMsg = ":" + client.nickname + "!" + client.username + "@" + client.hostname + 
                           " INVITE " + targetNick + " :" + channel + "\r\n";
    server->sendReply(targetFd, inviteMsg);
    
    // Confirm to inviter
    server->sendReply(fd, server->formatServerReply(fd, "341 " + client.nickname + " " + targetNick + " " + channel));
}
