#include "Channel.hpp"

Channel::Channel(std::string channelName) : name(channelName), operator_fd(-1), topicRestricted(false), inviteOnly(false) {}

Channel::Channel() : name(""), operator_fd(-1) {} 


void Channel::addMember(int client_fd, const std::string& nickname, const std::string& username) {
    members.insert(client_fd);
    memberNicknames[client_fd] = nickname;
    memberUsernames[client_fd] = username;
}


std::string Channel::getMembersList() {
    std::string membersList;
    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (operators.find(*it) != operators.end()) {
            membersList += "@";
        }
        membersList += memberNicknames[*it] + " ";
    }
    if (!membersList.empty()) {
        membersList.erase(membersList.size() - 1);
    }
    return membersList;
}


void Channel::removeMember(int client_fd) {
    members.erase(client_fd);
    
    
    std::string nickname;
    for (std::map<int, std::string>::const_iterator it = memberNicknames.begin(); it != memberNicknames.end(); ++it) {
        if (it->first == client_fd) {
            nickname = it->second;
            break;
        }
    }
    
    invitedUsers.erase(nickname);
    
    if (client_fd == operator_fd && !members.empty()) {
        operator_fd = *members.begin();
    }

    memberNicknames.erase(client_fd);
    memberUsernames.erase(client_fd);
}


void Channel::makeOperator(int client_fd) {
    operators.insert(client_fd);
}

bool Channel::isMember(int client_fd) const {
        return members.find(client_fd) != members.end();
    }


void Channel::sendMessageToChannel(const std::string& message, int sender_fd) {
    std::string sender_nickname = memberNicknames[sender_fd];
    std::string sender_username = memberUsernames[sender_fd];

    std::string ircMessage = ":" + sender_nickname + "!" + sender_username + "@localhost PRIVMSG " + name + " :" + message + "\r\n";

    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (*it != sender_fd) {
            send(*it, ircMessage.c_str(), ircMessage.length(), 0);
        }
    }
}


int Channel::getFdByNickname(const std::string &nickname) const {
    for (std::map<int, std::string>::const_iterator it = memberNicknames.begin(); it != memberNicknames.end(); ++it) {
        if (it->second == nickname) {
            return it->first;
        }
    }
    return -1;
}


void Channel::inviteUser(const std::string& nickname) {
    invitedUsers.insert(nickname);
}


void Channel::setTopic(const std::string& newTopic) {
    topic = newTopic;
}


std::string Channel::getTopic() const {
    return topic;
}


bool Channel::isTopicRestricted() const {
    return topicRestricted;
}


bool Channel::isInviteOnly() const {
    return inviteOnly;
}


bool Channel::isInvited(const std::string& nickname) const {
    return invitedUsers.find(nickname) != invitedUsers.end();
}


std::string Channel::getNicknameForFd(int client_fd) {
    if (memberNicknames.find(client_fd) != memberNicknames.end()) {
        return memberNicknames[client_fd];
    }
    return "";
}


std::string Channel::getChannelKey() const {
    return channelKey;
}


bool Channel::isOperator(int client_fd) const {
    return operators.find(client_fd) != operators.end();
}


void Channel::broadcast(const std::string& message) {
    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        send(*it, message.c_str(), message.size(), 0);
    }
}


int Channel::getUserLimit() const 
{ 
    return userLimit; 
}


int Channel::getMemberCount() const 
{ 
    return members.size(); 
}


void Channel::setMode(const std::string& mode, const std::string& param, int client_fd) {
    std::string logMessage;

    if (mode == "+i") {
        inviteOnly = true;
        logMessage = "Invite-only mode enabled.";
    } else if (mode == "-i") {
        inviteOnly = false;
        logMessage = "Invite-only mode disabled.";
    } else if (mode == "+t") {
        topicRestricted = true;
        logMessage = "Topic-restricted mode enabled.";
    } else if (mode == "-t") {
        topicRestricted = false;
        logMessage = "Topic-restricted mode disabled.";
    } else if (mode == "+k") {
        if (param.empty()) {
            std::string errorMsg = ":irc.localhost 461 " + getNicknameForFd(client_fd) +
                                   " MODE :Not enough parameters for +k\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        channelKey = param;
        logMessage = "Password set: " + channelKey;
    } else if (mode == "-k") {
        channelKey = "";
        logMessage = "Password removed.";
    } else if (mode == "+o") {
        int user_fd = getFdByNickname(param);
        if (user_fd == -1) {
            std::string errorMsg = ":irc.localhost 401 " + getNicknameForFd(client_fd) +
                                " " + param + " :No such nick/channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        operators.insert(user_fd);
        std::string modeMsg = ":" + getNicknameForFd(client_fd) + "!" + getNicknameForFd(client_fd) +
                            "@localhost MODE " + name + " +o " + param + "\r\n";
        broadcast(modeMsg);
        logMessage = "User " + param + " is now an operator.";
    } else if (mode == "-o") {
        int user_fd = getFdByNickname(param);
        if (user_fd == -1) {
            std::string errorMsg = ":irc.localhost 401 " + getNicknameForFd(client_fd) +
                                   " " + param + " :No such nick/channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        if (operators.find(client_fd) != operators.end() && client_fd != user_fd) {
            std::string errorMsg = ":irc.localhost 482 " + getNicknameForFd(client_fd) +
                                   " " + name + " :You cannot remove another operator\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        operators.erase(user_fd);
        std::string demoteMsg = ":irc.localhost 341 " + getNicknameForFd(client_fd) + " " + param + " " + name + " :Operator privileges removed\r\n";
        send(user_fd, demoteMsg.c_str(), demoteMsg.size(), 0);
        logMessage = "User " + param + " is no longer an operator.";
    } else if (mode == "+l") {
        if (param.empty() || atoi(param.c_str()) <= 0) {
            std::string errorMsg = ":irc.localhost 461 " + getNicknameForFd(client_fd) +
                                   " MODE :Invalid parameter for +l\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        userLimit = atoi(param.c_str());
        logMessage = "User limit set to " + param;
    } else if (mode == "-l") {
        userLimit = 0;
        logMessage = "User limit removed.";
    } else {
        std::string errorMsg = ":irc.localhost 472 " + getNicknameForFd(client_fd) +
                               " " + mode + " :is unknown mode char for " + name + "\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    if (!logMessage.empty()) {
        std::cout << "Setting mode " << mode << " on channel " << name << ": " << logMessage << std::endl;
    }
}
