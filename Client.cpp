#include "Client.hpp"

Client::Client(int fd) {
    this->fd = fd;
    this->authenticated = false;
    this->hasNick = false;
    this->hasUser = false;
    this->welcomeSent = false;
}

Client::Client() {
    this->fd = -1;
    this->authenticated = false;
    this->hasNick = false;
    this->hasUser = false;
}

bool Client::isAuthenticated() const {
    return this->authenticated;
}

void Client::setAuthenticated(bool value) {
    this->authenticated = value;
}

bool Client::hasNickname() const {
    return hasNick;
}

bool Client::hasUsername() const {
    return hasUser;
}

void Client::setNickname(const std::string &nickname) {
    if (nickname.empty()) {
        send(fd, "ERROR: Nickname cannot be empty\n", 30, 0);
        return;
    }
    this->nickname = nickname;
    this->hasNick = true;
    std::cout << "Client " << fd << " set nickname to " << nickname << std::endl;
}


void Client::setUsername(const std::string &username) {
    if (username.empty()) {
        send(fd, "ERROR: Username cannot be empty\n", 30, 0);
        return;
    }
    this->username = username;
    this->hasUser = true;
    this->authenticated = true;
    std::cout << "Client " << fd << " set username to " << username << std::endl;
}

int Client::getFd() const { 
    return fd; 
}

std::string Client::getNickname() const {
    return nickname; 
}

std::string Client::getUsername() const {
    return username; 
}

void Client::appendToBuffer(const std::string &data) {
    buffer += data;
}

std::string Client::getBuffer() const {
    return buffer;
}

void Client::clearBuffer(size_t pos) {
    buffer.erase(0, pos + 1);
}

void Client::setCurrentChannel(const std::string &channel) {
    currentChannel = channel;
}

std::string Client::getCurrentChannel() const {
    return currentChannel;
}

bool Client::hasSentWelcome() const {
    return welcomeSent;
}

void Client::setSentWelcome(bool val) { 
    welcomeSent = val;
}