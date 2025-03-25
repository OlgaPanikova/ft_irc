#include "ChatServer.hpp"

ChatServer::ChatServer(int port, const std::string &password) 
        : serverPassword(password), serverPort(port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::perror("Socket failed");
        exit(1);
    }

    setNonBlocking(server_fd);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(serverPort);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(1);
    }

    pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);

    std::cout << "Server started on port " << serverPort << std::endl;
}


void ChatServer::setNonBlocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
    }
}


ChatServer::~ChatServer() {
    for (size_t i = 0; i < fds.size(); i++) {
        close(fds[i].fd);
    }
}

void ChatServer::run() {
    while (true) {
        int ret = poll(&fds[0], fds.size(), -1);
        if (ret < 0) {
            perror("Poll error");
            break;
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    handleNewConnection();
                } else {
                    handleClientMessage(fds[i].fd);
                }
            }
        }
    }
}

void ChatServer::handleNewConnection() {
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Accept failed");
        return;
    }
    setNonBlocking(client_fd);

    std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;

    std::string passwordPrompt = ":irc.localhost NOTICE * :Please enter the password using PASS <password>.\r\n";
    send(client_fd, passwordPrompt.c_str(), passwordPrompt.size(), 0);

    pollfd new_pollfd;
    new_pollfd.fd = client_fd;
    new_pollfd.events = POLLIN;
    new_pollfd.revents = 0;

    fds.push_back(new_pollfd);
    Client newClient(client_fd);
    newClient.setAuthenticated(false);
    clients[client_fd] = newClient;
}

void ChatServer::handleClientMessage(int client_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_read <= 0) {
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        handleClientDisconnect(client_fd);
        return;
    }

    Client &client = clients[client_fd];
    std::string data(buffer, bytes_read);
    client.appendToBuffer(data);

    while (true) {
        size_t pos = client.getBuffer().find('\n');
        if (pos == std::string::npos) {
            break;
        }
        std::string message = client.getBuffer().substr(0, pos);
        client.clearBuffer(pos);
        if (!message.empty() && message[message.size() - 1] == '\r') {
            message.erase(message.size() - 1);
        }
        processCompleteMessage(client_fd, message);
    }
}

void ChatServer::handleClientDisconnect(int client_fd) {
    std::cout << "Client disconnected (fd=" << client_fd << ")\n";
    close(client_fd);
    for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
        if (it->fd == client_fd) {
            fds.erase(it);
            break;
        }
    }
    clients.erase(client_fd);
}

void ChatServer::processCompleteMessage(int client_fd, const std::string &message) {
    Client &client = clients[client_fd];

    std::istringstream iss(message);
    std::string command;
    iss >> command;

    std::string param;
    std::getline(iss, param);
    if (!param.empty() && param[0] == ' ') {
        param.erase(0, 1);
    }

    while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
        command.erase(0, 1);
    }
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "PING") {
        std::string token = param;
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty())
            token += rest;
        if (token.empty())
            token = ":irc.localhost";
        if (token[0] != ':')
            token = ":" + token;
        std::string response = "PONG " + token + "\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    if (!client.isAuthenticated()) {
        if (command == "PASS") {
            if (param.empty()) {
                std::string response = ":irc.localhost 461 * PASS :Not enough parameters.\r\n";
                send(client_fd, response.c_str(), response.size(), 0);
                return;
            }
            if (param == serverPassword) {
                client.setAuthenticated(true);
                std::string response = ":irc.localhost NOTICE * :Password accepted. Please enter NICK and USER.\r\n";
                send(client_fd, response.c_str(), response.size(), 0);
            } else {
                std::string response = ":irc.localhost 464 * :Incorrect password.\r\n";
                send(client_fd, response.c_str(), response.size(), 0);
                handleClientDisconnect(client_fd);
            }
        } else {
            std::string response = ":irc.localhost NOTICE * :Please enter the password using PASS <password>\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
        }
        return;
    }

    if (command == "NICK") {
        if (param.empty()) {
            std::string response = ":irc.localhost 431 * :No nickname given\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }
        client.setNickname(param);
    }

    if (command == "USER") {
        handleUSERCommand(client_fd, param, client);
    }

    if (!client.hasSentWelcome() && client.hasNickname() && client.hasUsername()) {
        client.setSentWelcome(true);
        std::string welcomeMsg = ":irc.localhost 001 " + client.getNickname() + " :Welcome to the IRC server!\r\n";
        std::string motdStart = ":irc.localhost 375 " + client.getNickname() + " :- IRC Message of the Day -\r\n";
        std::string motdEnd = ":irc.localhost 376 " + client.getNickname() + " :End of /MOTD command.\r\n";
        send(client_fd, welcomeMsg.c_str(), welcomeMsg.size(), 0);
        send(client_fd, motdStart.c_str(), motdStart.size(), 0);
        send(client_fd, motdEnd.c_str(), motdEnd.size(), 0);
    }

    if (command != "PASS" && command != "NICK" && command != "USER") {
        if (!client.hasNickname() || !client.hasUsername()) {
            std::string response = ":irc.localhost 451 * :You have not registered\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }
    }
    if (isCommand(command)) {
        processCommand(client_fd, message);
    } else {
        std::string response = ":irc.localhost 421 * " + command + " :Unknown command\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

std::vector<std::string> splitParams(const std::string &s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
        if (token[0] == ':') {
            std::string rest;
            std::getline(iss, rest);
            if (!rest.empty()) {
                tokens.back() += rest;
            }
            break;
        }
    }
    return tokens;
}

void ChatServer::handleUSERCommand(int client_fd, const std::string &param, Client &client) {
    printf("%s\n", param.c_str());
    std::vector<std::string> tokens = splitParams(param);
    if (tokens.size() < 4) {
        std::string response = ":irc.localhost 461 * USER :Not enough parameters\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }
    
    std::string username = tokens[0];
    std::string mode = tokens[1];
    std::string unused = tokens[2];
    std::string realname = tokens[3];
    if (!realname.empty() && realname[0] == ':') {
        realname.erase(0, 1);
    }
    
    for (size_t i = 4; i < tokens.size(); i++) {
        realname += " " + tokens[i];
    }
    
    if (username.empty()) {
        std::string response = ":irc.localhost 461 * USER :Invalid username\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }
    
    client.setUsername(username);
}


bool ChatServer::isCommand(const std::string &command) {
    static std::set<std::string> commands;
    
    if (commands.empty()) {
        commands.insert("NICK");
        commands.insert("USER");
        commands.insert("JOIN");
        commands.insert("PART");
        commands.insert("PRIVMSG");
        commands.insert("NOTICE");
        commands.insert("KICK");
        commands.insert("INVITE");
        commands.insert("TOPIC");
        commands.insert("MODE");
        commands.insert("PING");
        commands.insert("PONG");
        commands.insert("QUIT");
    }
    return commands.find(command) != commands.end();
}

void ChatServer::processCommand(int client_fd, const std::string &message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
        command.erase(0, 1);
    }

    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "NICK" || command == "USER" || command == "PASS") {
        return;
    }
    if (command == "JOIN") {
        processJoinCommand(client_fd, iss);
    } else if (command == "PRIVMSG") {
        processPrivMsgCommand(client_fd, iss);
    } else if (command == "KICK") {
        processKickCommand(client_fd, iss);
    } else if (command == "INVITE") {
        processInviteCommand(client_fd, iss);
    } else if (command == "TOPIC") {
        processTopicCommand(client_fd, iss);
    } else if (command == "MODE") {
        processModeCommand(client_fd, iss);
    } else if (command == "PART") {
        processPartCommand(client_fd, iss);
    } else if (command == "NOTICE") {
        processNoticeCommand(client_fd, iss);
    } else if (command == "QUIT") {
        processQuitCommand(client_fd, iss);
    } else {
        std::string errorMsg = ":irc.localhost 421 " + clients[client_fd].getNickname() +
                               " " + command + " :Unknown command\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
    }
}


int ChatServer::getFdByNickname(const std::string &nick) {
    for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.getNickname() == nick) {
            return it->first;
        }
    }
    return -1;
}

