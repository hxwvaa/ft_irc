#ifndef PARCER_HPP
#define PARCER_HPP

#include <string>
#include <vector>

// Forward declaration
class Server;

std::vector<std::string> parseMessage(const std::string& message);
std::vector<std::string> splitByComma(const std::string& str);
bool stringToInt(const std::string& str, int& result);
bool isValidNickname(const std::string& nick);

// IRC command handlers
void handlePass(Server* server, int fd, const std::vector<std::string>& params);
void handleNick(Server* server, int fd, const std::vector<std::string>& params);
void handleUser(Server* server, int fd, const std::vector<std::string>& params);
void handleJoin(Server* server, int fd, const std::vector<std::string>& params);
void handlePrivMsg(Server* server, int fd, const std::vector<std::string>& params);
void handleQuit(Server* server, int fd, const std::vector<std::string>& params);
void handlePing(Server* server, int fd, const std::vector<std::string>& params);
void handlePart(Server* server, int fd, const std::vector<std::string>& params);
void handleMode(Server* server, int fd, const std::vector<std::string>& params);
void handleWho(Server* server, int fd, const std::vector<std::string>& params);
void handleList(Server* server, int fd, const std::vector<std::string>& params);
void handleKick(Server* server, int fd, const std::vector<std::string>& params);
void handleInvite(Server* server, int fd, const std::vector<std::string>& params);

#endif // PARCER_HPP
