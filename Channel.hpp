#pragma once
#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <iostream>
#include <sstream>
#include <set>
#include "Client.hpp"
#include "ChatServer.hpp"

class Client;
class ChatServer;

class Channel {
public:
    std::string name;
    std::set<int> members; // Список клиентов (fd)
    std::set<std::string> invitedUsers;
    std::set<int> operators;
    std::map<int, std::string> memberNicknames;
    std::map<int, std::string> memberUsernames;
    std::string topic;
    std::string channelKey;
    int operator_fd; // Оператор канала
    int userLimit;
    bool topicRestricted;
    bool inviteOnly;

    Channel(std::string channelName);
    Channel();
    void addMember(int client_fd, const std::string& nickname, const std::string& username);
    std::string getMembersList();
    void removeMember(int client_fd);
    void makeOperator(int client_fd);
    bool isMember(int client_fd) const;
    void sendMessageToChannel(const std::string &message, int sender_fd);
    int getFdByNickname(const std::string &nickname) const;
    void inviteUser(const std::string& nickname);
    void setTopic(const std::string& newTopic);
    std::string getTopic() const;
    bool isTopicRestricted() const;
    bool isInviteOnly() const;
    bool isInvited(const std::string& nickname) const;
    std::string getNicknameForFd(int client_fd);
    std::string getChannelKey() const;
    bool isOperator(int client_fd) const;
    void broadcast(const std::string& message);
    int getUserLimit() const;
    int getMemberCount() const;
    void setMode(const std::string& mode, const std::string& param, int client_fd);
};


#endif