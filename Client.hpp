#pragma once

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <iostream>
#include <sys/socket.h>

class Client {
private:
    std::string nickname;
    std::string username;
    std::string currentChannel;
    std::string buffer;
    bool authenticated;
    bool hasNick;
    bool hasUser;
    bool welcomeSent;
    int fd;

public:
    Client(int fd);
    Client();

    bool isAuthenticated() const;
    void setAuthenticated(bool value);

    bool hasNickname() const;
    bool hasUsername() const;
    void setNickname(const std::string &nickname);
    void setUsername(const std::string &username);

    int getFd() const;
    std::string getNickname() const;
    std::string getUsername() const; 

    void appendToBuffer(const std::string &data);
    std::string getBuffer() const;
    void clearBuffer(size_t pos);

    void setCurrentChannel(const std::string &channel);
    std::string getCurrentChannel() const;

    bool hasSentWelcome() const;
    void setSentWelcome(bool val);
};

#endif