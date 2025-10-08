
#include "parcer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdlib.h> // atoi

// ===== helpers =====
std::string Parcer::trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) --b;
    return s.substr(a, b-a);
}
void Parcer::to_upper(std::string& s) {
    for (size_t i=0;i<s.size();++i) s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
}
bool Parcer::isChannel(const std::string& name) {
    return !name.empty() && (name[0]=='#' || name[0]=='&');
}

// ===== parity helpers =====
bool Parcer::isValidNicknameLocal(const std::string& nickname) {
    if (nickname.empty() || nickname.length() > 30) return false;

    // First char must be a letter
    char first = nickname[0];
    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
        return false;

    // Allowed: letters, digits, _ - [ ] { } \ ` |
    for (size_t i = 1; i < nickname.length(); ++i) {
        char c = nickname[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '[' ||
              c == ']' || c == '{' || c == '}' || c == '\\' || c == '`' || c == '|')) {
            return false;
        }
    }
    return true;
}

bool Parcer::isValidChannelNameLocal(const std::string& channelName) {
    if (channelName.empty() || channelName.length() < 2 || channelName.length() > 200)
        return false;
    char p = channelName[0];
    if (p != '#' && p != '&')
        return false;
    // Invalid chars: space, comma, CR, LF, NUL
    for (size_t i = 1; i < channelName.length(); ++i) {
        unsigned char c = channelName[i];
        if (c == ' ' || c == ',' || c == '\r' || c == '\n' || c == '\0')
            return false;
    }
    return true;
}

bool Parcer::safeAtoi(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* endp = 0;
    long v = strtol(s.c_str(), &endp, 10);
    if (!endp || *endp != '\0') return false; // extra chars
    if (v > INT_MAX || v < INT_MIN) return false;
    out = static_cast<int>(v);
    return true;
}

void Parcer::tryFinishRegistration(int fd) {
    if (!_A->isRegistered(fd)) {
        if (!_A->checkPasswordGiven(fd)) return;
        if (_A->getNick(fd).empty()) return;
        if (_A->getUser(fd).empty()) return;
        _A->setRegistered(fd, true);
        _A->sendNumeric(fd, 001, _A->serverName() + " :Welcome to the IRC network " + _A->getNick(fd));
        _A->sendNumeric(fd, 002, ":Your host is " + _A->serverName());
        _A->sendNumeric(fd, 003, ":This server was created just now");
    }
}

// ===== parse =====
IRCCommand Parcer::parse(const std::string& raw) const {
    IRCCommand out;
    std::string line = trim(raw);
    if (line.empty()) return out;
    size_t pos=0;
    if (line[pos]==':') {
        ++pos;
        size_t sp = line.find(' ', pos);
        if (sp==std::string::npos) return out;
        out.prefix = line.substr(pos, sp-pos);
        pos = sp+1;
        while (pos<line.size() && line[pos]==' ') ++pos;
    }
    size_t cmd_end = line.find(' ', pos);
    if (cmd_end==std::string::npos) {
        out.command = line.substr(pos);
        to_upper(out.command);
        return out;
    }
    out.command = line.substr(pos, cmd_end-pos);
    to_upper(out.command);
    pos = cmd_end+1;
    while (pos<line.size()) {
        while (pos<line.size() && line[pos]==' ') ++pos;
        if (pos>=line.size()) break;
        if (line[pos]==':') {
            out.params.push_back(line.substr(pos+1));
            break;
        } else {
            size_t next = line.find(' ', pos);
            if (next==std::string::npos) { out.params.push_back(line.substr(pos)); break; }
            out.params.push_back(line.substr(pos, next-pos));
            pos = next+1;
        }
    }
    if (out.params.size() > 15){
        out.params.resize(15);}
    return out;
}

// ===== handle (router) =====
void Parcer::handle(int fd, const IRCCommand& cmd) {
    const std::string& C = cmd.command;
    const std::vector<std::string>& P = cmd.params;
    if (C=="PASS") { do_PASS(fd, P); return; }
    if (C=="NICK") { do_NICK(fd, P); return; }
    if (C=="USER") { do_USER(fd, P); return; }
    if (C=="PING") { do_PING(fd, P); return; }
    if (C=="PONG") { do_PONG(fd, P); return; }
    if (C=="JOIN") { do_JOIN(fd, P); return; }
    if (C=="PART") { do_PART(fd, P); return; }
    if (C=="PRIVMSG") { do_PRIVMSG(fd, P, false); return; }
    if (C=="NOTICE") { do_PRIVMSG(fd, P, true); return; }
    if (C=="TOPIC") { do_TOPIC(fd, P); return; }
    if (C=="INVITE") { do_INVITE(fd, P); return; }
    if (C=="KICK") { do_KICK(fd, P); return; }
    if (C=="MODE") { do_MODE(fd, P); return; }
    if (C=="LIST") { do_LIST(fd, P); return; }
    if (C=="WHO") { do_WHO(fd, P); return; }
    if (C=="QUIT") { do_QUIT(fd, P); return; }
    _A->sendNumeric(fd, 421, cmd.command + " :Unknown command");
}

// ===== individual commands =====
void Parcer::do_PASS(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 461, "PASS :Not enough parameters"); return; }
    if (_A->isRegistered(fd)) { _A->sendNumeric(fd, 462, ":You may not reregister"); return; }
    const std::string& given = P[0];
    if (!given.empty() && given == _A->serverPassword()) {
        _A->setPasswordGiven(fd, true);
        tryFinishRegistration(fd);
    } else {
        _A->sendNumeric(fd, 464, ":Password incorrect");
    }
}

void Parcer::do_NICK(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 431, ":No nickname given"); return; }
    std::string newNick = P[0];
    
    if (!isValidNicknameLocal(newNick)) { _A->sendNumeric(fd, 432, newNick + " :Erroneous nickname"); return; }
    // (optional)
    if (!_A->isValidNick(newNick)) { _A->sendNumeric(fd, 432, newNick + " :Erroneous nickname"); return; }

    int other = _A->fdByNick(newNick);
    if (other!=-1 && other!=fd) { _A->sendNumeric(fd, 433, newNick + " :Nickname is already in use"); return; }
    std::string old = _A->getNick(fd);
    _A->setNick(fd, newNick);
    if (!old.empty() && old!=newNick) {
        std::string msg = ":" + old + " NICK :" + newNick + "\r\n";
        _A->sendToFd(fd, msg);
    }
    tryFinishRegistration(fd);
}

void Parcer::do_USER(int fd, const std::vector<std::string>& P) {
    if (P.size()<4) { _A->sendNumeric(fd, 461, "USER :Not enough parameters"); return; }
    if (_A->isRegistered(fd)) { _A->sendNumeric(fd, 462, ":You may not reregister"); return; }
    _A->setUser(fd, P[0]);
    _A->setRealname(fd, P[3]);
    tryFinishRegistration(fd);
}

void Parcer::do_PING(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 409, ":No origin specified"); return; }
    _A->sendToFd(fd, ":" + _A->serverName() + " PONG " + _A->serverName() + " :" + P[0] + "\r\n");
}
void Parcer::do_PONG(int, const std::vector<std::string>&) { }

void Parcer::do_JOIN(int fd, const std::vector<std::string>& P) {
    if (!_A->isRegistered(fd)) { _A->sendNumeric(fd, 451, ":You have not registered"); return; }
    if (P.empty()) { _A->sendNumeric(fd, 461, "JOIN :Not enough parameters"); return; }
    std::vector<std::string> chans; std::vector<std::string> keys;
    std::stringstream ss1(P[0]); std::string tok;
    while (std::getline(ss1, tok, ',')) if (!tok.empty()) chans.push_back(tok);
    if (P.size()>1) { std::stringstream ss2(P[1]); while (std::getline(ss2, tok, ',')) keys.push_back(tok); }
    for (size_t i=0;i<chans.size();++i) {
        std::string c = chans[i];
        if (!isValidChannelNameLocal(c)) {_A->sendNumeric(fd, 403, c + " :No such channel"); continue; }
        bool exists = _A->channelExists(c);
        if (!exists) _A->createChannel(c, fd);
        if (_A->mode_i(c) && !_A->isInvited(c, fd)) { _A->sendNumeric(fd, 473, c + " :Cannot join channel (+i)"); continue; }
        if (_A->has_key(c)) {
            std::string key = (i<keys.size()?keys[i]:"");
            if (key != _A->key(c)) { _A->sendNumeric(fd, 475, c + " :Cannot join channel (+k)"); continue; }
        }
        int lim = _A->limit(c);
        if (lim>0 && _A->channelSize(c) >= (size_t)lim) { _A->sendNumeric(fd, 471, c + " :Cannot join channel (+l)"); continue; }

        if (!_A->isInChannel(fd, c)) _A->joinChannel(fd, c);

        std::string nick = _A->getNick(fd);
        _A->broadcastToChannel(c, ":" + nick + " JOIN :" + c + "\r\n", -1);
        if (!_A->getTopic(c).empty())
            _A->sendNumeric(fd, 332, c + " :" + _A->getTopic(c));
        // 353 and 366 (names)
        std::string nl = "= " + c + " :";
        const std::set<int>& mem = _A->channelMembers(c);
        for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
            int mfd = *it;
            if (_A->isChanOper(mfd, c)) nl += "@";
            nl += _A->getNick(mfd);
            nl += " ";
        }
        _A->sendNumeric(fd, 353, nl);
        _A->sendNumeric(fd, 366, c + " :End of /NAMES list.");
    }
}

void Parcer::do_PART(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 461, "PART :Not enough parameters"); return; }
    std::string chan = P[0];
    std::string reason = (P.size()>1?P[1]:"");
    if (!_A->channelExists(chan)) { _A->sendNumeric(fd, 403, chan + " :No such channel"); return; }
    if (!_A->isInChannel(fd, chan)) { _A->sendNumeric(fd, 442, chan + " :You're not on that channel"); return; }
    _A->broadcastToChannel(chan, ":" + _A->getNick(fd) + " PART " + chan + (reason.empty()?"":" :"+reason) + "\r\n", -1);
    _A->partChannel(fd, chan, reason);
}

void Parcer::do_PRIVMSG(int fd, const std::vector<std::string>& P, bool isNotice) {
    if (P.size()<2) { _A->sendNumeric(fd, 461, (isNotice?"NOTICE":"PRIVMSG") + std::string(" :Not enough parameters")); return; }
    std::string target = P[0];
    std::string text = P[1];
    std::string fromNick = _A->getNick(fd);
    std::string msg = ":" + fromNick + (isNotice?" NOTICE ":" PRIVMSG ") + target + " :" + text + "\r\n";
    if (isChannel(target)) {
        if (!_A->channelExists(target)) { _A->sendNumeric(fd, 403, target + " :No such channel"); return; }
        if (!_A->isInChannel(fd, target)) { _A->sendNumeric(fd, 404, target + " :Cannot send to channel"); return; }
        _A->broadcastToChannel(target, msg, -1);
    } else {
        int tfd = _A->fdByNick(target);
        if (tfd==-1) { _A->sendNumeric(fd, 401, target + " :No such nick/channel"); return; }
        if (isNotice) _A->noticeToFd(tfd, msg);
        else _A->sendToFd(tfd, msg);
    }
}

void Parcer::do_TOPIC(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 461, "TOPIC :Not enough parameters"); return; }
    std::string chan = P[0];
    if (!_A->channelExists(chan)) { _A->sendNumeric(fd, 403, chan + " :No such channel"); return; }
    if (P.size()==1) {
        std::string t = _A->getTopic(chan);
        if (t.empty()) _A->sendNumeric(fd, 331, chan + " :No topic is set");
        else _A->sendNumeric(fd, 332, chan + " :" + t);
        return;
    }
    if (_A->mode_t(chan) && !_A->isChanOper(fd, chan)) { _A->sendNumeric(fd, 482, chan + " :You're not channel operator"); return; }
    std::string newTopic = P[1];
    _A->setTopic(chan, newTopic);
    _A->broadcastToChannel(chan, ":" + _A->getNick(fd) + " TOPIC " + chan + " :" + newTopic + "\r\n", -1);
}

void Parcer::do_INVITE(int fd, const std::vector<std::string>& P) {
    if (P.size()<2) { _A->sendNumeric(fd, 461, "INVITE :Not enough parameters"); return; }
    std::string nick = P[0], chan = P[1];
    if (!_A->channelExists(chan)) { _A->sendNumeric(fd, 403, chan + " :No such channel"); return; }
    if (!_A->isInChannel(fd, chan)) { _A->sendNumeric(fd, 442, chan + " :You're not on that channel"); return; }
    if (!_A->isChanOper(fd, chan)) { _A->sendNumeric(fd, 482, chan + " :You're not channel operator"); return; }
    int tfd = _A->fdByNick(nick);
    if (tfd==-1) { _A->sendNumeric(fd, 401, nick + " :No such nick/channel"); return; }
    _A->addInvite(chan, tfd);
    _A->sendToFd(tfd, ":" + _A->getNick(fd) + " INVITE " + nick + " :" + chan + "\r\n");
    _A->sendNumeric(fd, 341, nick + " " + chan);
}

void Parcer::do_KICK(int fd, const std::vector<std::string>& P) {
    if (P.size()<2) { _A->sendNumeric(fd, 461, "KICK :Not enough parameters"); return; }
    std::string chan = P[0], nick = P[1];
    if (!_A->channelExists(chan)) { _A->sendNumeric(fd, 403, chan + " :No such channel"); return; }
    if (!_A->isChanOper(fd, chan)) { _A->sendNumeric(fd, 482, chan + " :You're not channel operator"); return; }
    int tfd = _A->fdByNick(nick);
    if (tfd==-1 || !_A->isInChannel(tfd, chan)) { _A->sendNumeric(fd, 441, nick + " " + chan + " :They aren't on that channel"); return; }
    std::string reason = (P.size()>2?P[2]:"");
    _A->broadcastToChannel(chan, ":" + _A->getNick(fd) + " KICK " + chan + " " + nick + (reason.empty()?"":" :"+reason) + "\r\n", -1);
    _A->partChannel(tfd, chan, "KICK");
}

void Parcer::do_MODE(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 461, "MODE :Not enough parameters"); return; }
    std::string target = P[0];
    if (isChannel(target)) {
        if (!_A->channelExists(target)) { _A->sendNumeric(fd, 403, target + " :No such channel"); return; }
        if (P.size()==1) {
            std::string ms = "+";
            if (_A->mode_i(target)) ms += "i";
            if (_A->mode_t(target)) ms += "t";
            if (_A->has_key(target)) ms += "k";
            if (_A->limit(target)>0) ms += "l";
            _A->sendNumeric(fd, 324, target + " " + ms);
            return;
        }
        if (!_A->isChanOper(fd, target)) { _A->sendNumeric(fd, 482, target + " :You're not channel operator"); return; }
        std::string modes = P[1];
        bool adding = true;
        size_t argi = 2;
        for (size_t i=0;i<modes.size();++i) {
            char ch = modes[i];
            if (ch=='+'){ adding=true; continue; }
            if (ch=='-'){ adding=false; continue; }
            if (ch=='i'){ _A->set_mode_i(target, adding); continue; }
            if (ch=='t'){ _A->set_mode_t(target, adding); continue; }
            if (ch=='k'){
                if (adding) {
                    if (argi>=P.size()) { _A->sendNumeric(fd, 461, "MODE :Not enough parameters"); return; }
                    _A->set_key(target, P[argi++]);
                } else {
                    _A->unset_key(target);
                }
                continue;
            }
            if (ch=='l'){
                if (adding) {
                    if (argi>=P.size()) { _A->sendNumeric(fd, 461, "MODE :Not enough parameters"); return; }
                    int limVal = 0;
                    if (argi >= P.size() || !safeAtoi(P[argi], limVal)) {
                        _A->sendNumeric(fd, 461, "MODE :Not enough parameters"); // or "MODE :Invalid limit"
                        return;
                    }
                    ++argi;
                    if (limVal < 0) limVal = 0;
                    _A->set_limit(target, limVal);
                } else {
                    _A->set_limit(target, 0);
                }
                continue;
            }
            if (ch=='o'){
                if (argi>=P.size()) { _A->sendNumeric(fd, 461, "MODE :Not enough parameters"); return; }
                int tfd = _A->fdByNick(P[argi++]);
                if (tfd==-1 || !_A->isInChannel(tfd, target)) { _A->sendNumeric(fd, 441, P[argi-1] + " " + target + " :They aren't on that channel"); return; }
                _A->setChanOper(fd, target, tfd, adding);
                continue;
            }
            _A->sendNumeric(fd, 472, std::string(1,ch) + " :is unknown mode char to me");
        }
        _A->broadcastToChannel(target, ":" + _A->getNick(fd) + " MODE " + target + " " + P[1] + "\r\n", -1);
    } else {
        _A->sendNumeric(fd, 502, ":Cannot change mode for other users");
    }
}

void Parcer::do_LIST(int fd, const std::vector<std::string>&) {
    _A->sendNumeric(fd, 321, "Channel :Users Name");
    std::vector<std::string> chans = _A->listChannels();
    for (size_t i=0;i<chans.size();++i) {
        const std::string& c = chans[i];
        std::string ln = c + " " + (chans.size() ? "" : "") ;
        // size and topic
        std::stringstream ss; ss << c << " " << _A->channelSize(c) << " :" << _A->getTopic(c);
        _A->sendNumeric(fd, 322, ss.str());
    }
    _A->sendNumeric(fd, 323, ":End of /LIST");
}

void Parcer::do_WHO(int fd, const std::vector<std::string>& P) {
    if (P.empty()) { _A->sendNumeric(fd, 461, "WHO :Not enough parameters"); return; }
    std::string chan = P[0];
    if (!_A->channelExists(chan)) { _A->sendNumeric(fd, 403, chan + " :No such channel"); return; }
    const std::set<int>& mem = _A->channelMembers(chan);
    for (std::set<int>::const_iterator it = mem.begin(); it != mem.end(); ++it) {
        int mfd = *it;
        std::stringstream ln; ln << chan << " " << _A->getUser(mfd) << " host " << _A->serverName() << " " << _A->getNick(mfd) << " H :0 " << _A->getRealname(mfd);
        _A->sendNumeric(fd, 352, ln.str());
    }
    _A->sendNumeric(fd, 315, chan + " :End of /WHO list.");
}

void Parcer::do_QUIT(int fd, const std::vector<std::string>& P) {
    std::string reason = (P.empty()?"Client Quit":P[0]);
    _A->sendNumeric(fd, 221, ":Closing Link: " + _A->getNick(fd) + " (" + reason + ")");
}
