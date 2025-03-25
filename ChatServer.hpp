#pragma once
#ifndef CHATSERVER_HPP
#define CHATSERVER_HPP

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "Client.hpp"
#include "Channel.hpp"
#include <cstdio>
#include <cerrno>
#include <algorithm>


class Client;
class Channel;

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

class ChatServer {
private:
    int server_fd;
    std::string serverPassword;
    int serverPort;
    std::map<std::string, Channel> channels;
    std::map<int, Client> clients;
    std::vector<pollfd> fds;
    struct sockaddr_in server_addr;

    void setNonBlocking(int fd);
    void handleNewConnection();
    void handleClientMessage(int client_fd);
    void handleClientDisconnect(int client_fd);
    void processCompleteMessage(int client_fd, const std::string &message);
    void handleUSERCommand(int client_fd, const std::string &param, Client &client);
    void processCommand(int client_fd, const std::string &message);
    void processJoinCommand(int client_fd, std::istringstream &iss);
    void processPrivMsgCommand(int client_fd, std::istringstream &iss);
    void processKickCommand(int client_fd, std::istringstream &iss);
    void processInviteCommand(int client_fd, std::istringstream &iss);
    void processTopicCommand(int client_fd, std::istringstream &iss);
    void processModeCommand(int client_fd, std::istringstream &iss);
    void processPartCommand(int client_fd, std::istringstream &iss);
    void processNoticeCommand(int client_fd, std::istringstream &iss);
    void processQuitCommand(int client_fd, std::istringstream &iss);
    bool isCommand(const std::string &command);
    int getFdByNickname(const std::string &nick);


public:
    ChatServer(int port, const std::string &password);
    ~ChatServer();
    void run();
};

#endif