#include "ChatServer.hpp"

// Обработка команды JOIN: присоединение клиента к каналу.
// Извлекает имя канала и, при необходимости, ключ, затем проверяет условия (invite-only, лимит, пароль)
// и добавляет клиента в канал, отправляя соответствующие уведомления.
void ChatServer::processJoinCommand(int client_fd, std::istringstream &iss) {
    std::string channelName, key;
    iss >> channelName >> key;  // Читаем имя канала и возможный ключ (пароль)

    if (channelName.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + clients[client_fd].getNickname() +
                               " JOIN :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Если имя канала не начинается с '#', добавляем его.
    if (channelName[0] != '#') {
        channelName = "#" + channelName;
    }

    bool isNewChannel = (channels.find(channelName) == channels.end());

    if (isNewChannel) {
        // Создаем новый канал.
        channels.insert(std::make_pair(channelName, Channel(channelName)));
        std::cout << "Created new channel: " << channelName << std::endl;
    } else {
        Channel &chan = channels[channelName];

        // Проверяем invite-only режим.
        if (chan.isInviteOnly() && !chan.isInvited(clients[client_fd].getNickname())) {
            std::string errorMsg = ":irc.localhost 473 " + clients[client_fd].getNickname() +
                                   " " + channelName + " :Cannot join: Invite-only channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }

        // Проверяем лимит участников канала.
        if (chan.getUserLimit() > 0 && chan.getMemberCount() >= chan.getUserLimit()) {
            std::string errorMsg = ":irc.localhost 471 " + clients[client_fd].getNickname() +
                                   " " + channelName + " :Cannot join: Channel is full\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }

        // Проверяем, если канал защищен паролем, совпадает ли он с указанным.
        if (!chan.getChannelKey().empty() && chan.getChannelKey() != key) {
            std::string errorMsg = ":irc.localhost 475 " + clients[client_fd].getNickname() +
                                   " " + channelName + " :Cannot join: Incorrect channel key\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
    }

    // Проверяем, что у клиента установлен ник и юзернейм.
    std::string nickname = clients[client_fd].getNickname();
    if (nickname.empty()) {
        send(client_fd, "You must set a nickname before joining a channel.\r\n", 50, 0);
        return;
    }
    std::string username = clients[client_fd].getUsername();
    if (username.empty()) {
        send(client_fd, "You must set a username before joining a channel.\r\n", 50, 0);
        return;
    }

    // Добавляем клиента в канал и обновляем его текущий канал.
    channels[channelName].addMember(client_fd, nickname, username);
    clients[client_fd].setCurrentChannel(channelName);

    // Если канал новый, назначаем клиента оператором.
    if (isNewChannel) {
        channels[channelName].makeOperator(client_fd);
        std::string response = "You are now the channel operator.\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }

    // Отправляем подтверждение присоединения.
    std::string response = "Joined " + channelName + "\n";
    send(client_fd, response.c_str(), response.size(), 0);
    std::cout << "User " << client_fd << " joined channel: " << channelName << std::endl;

    // Рассылаем сообщение о входе в канал всем его участникам.
    std::string joinMsg = ":" + nickname + "!" + username + "@localhost JOIN " + channelName + "\r\n";
    channels[channelName].broadcast(joinMsg);

    // Отправляем тему канала.
    std::string topic = channels[channelName].getTopic();
    std::string topicMsg;
    if (!topic.empty()) {
        topicMsg = ":irc.localhost 332 " + nickname + " " + channelName + " :" + topic + "\r\n";
    } else {
        topicMsg = ":irc.localhost 331 " + nickname + " " + channelName + " :No topic is set\r\n";
    }
    send(client_fd, topicMsg.c_str(), topicMsg.size(), 0);

    // Отправляем список пользователей в канале.
    std::string namesList = ":irc.localhost 353 " + nickname + " = " + channelName + " :";
    namesList += channels[channelName].getMembersList();
    namesList += "\r\n";
    send(client_fd, namesList.c_str(), namesList.size(), 0);

    // Отправляем окончание списка пользователей.
    std::string endNames = ":irc.localhost 366 " + nickname + " " + channelName + " :End of /NAMES list\r\n";
    send(client_fd, endNames.c_str(), endNames.size(), 0);
}


// Обработка команды PRIVMSG: отправка личного сообщения или сообщения в канал.
// Если цель начинается с '#' или '&', сообщение отправляется в канал; иначе – как личное сообщение.
void ChatServer::processPrivMsgCommand(int client_fd, std::istringstream &iss) {
    std::string target, msg;
    iss >> target;              // Читаем цель: канал или ник.
    std::getline(iss, msg);     // Читаем остаток строки как сообщение.

    // Удаляем ведущие пробелы и символ ':' из начала сообщения.
    while (!msg.empty() && (msg[0] == ' ' || msg[0] == ':')) {
        msg.erase(0, 1);
    }

    std::cout << "PRIVMSG received - Target: '" << target << "', Message: '" << msg << "'" << std::endl;
    Client &client = clients[client_fd];

    if (target.empty() || msg.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " PRIVMSG :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Если цель является каналом.
    if (target[0] == '#' || target[0] == '&') {
        if (channels.find(target) != channels.end()) {
            if (!channels[target].isMember(client_fd)) {
                std::string errorMsg = ":irc.localhost 404 " + client.getNickname() +
                                       " " + target + " :Cannot send to channel\r\n";
                send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
                return;
            }
            channels[target].sendMessageToChannel(msg, client_fd);
        } else {
            std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                                   " " + target + " :No such channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        }
    }
    // Если цель – личное сообщение.
    else {
        int recipientFd = getFdByNickname(target);
        if (recipientFd == -1) {
            std::string errorMsg = ":irc.localhost 401 " + client.getNickname() +
                                   " " + target + " :No such nick/channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        } else {
            std::string senderPrefix = ":" + client.getNickname() + "!" +
                                       client.getUsername() + "@localhost";
            std::string messageToSend = senderPrefix + " PRIVMSG " + target +
                                        " :" + msg + "\r\n";
            send(recipientFd, messageToSend.c_str(), messageToSend.size(), 0);
        }
    }
}

// Обработка команды KICK: исключение пользователя из канала.
void ChatServer::processKickCommand(int client_fd, std::istringstream &iss) {
    std::string channel, target;
    iss >> channel >> target;

    Client &client = clients[client_fd];
    if (channel.empty() || target.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " KICK :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channel[0] != '#' && channel[0] != '&') {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channels.find(channel) == channels.end()) {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    Channel &chan = channels[channel];

    // Проверяем, является ли отправитель оператором канала.
    if (!chan.isOperator(client_fd)) {
        std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
                               " " + channel + " :You're not channel operator\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Ищем файловый дескриптор целевого пользователя по нику.
    int target_fd = -1;
    for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.getNickname() == target) {
            target_fd = it->first;
            break;
        }
    }

    if (target_fd == -1 || !chan.isMember(target_fd)) {
        std::string errorMsg = ":irc.localhost 441 " + client.getNickname() +
                               " " + target + " " + channel + " :They aren't on that channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Формируем сообщение о кике и рассылаем его участникам канала.
    std::string comment = target; // Комментарий можно расширить при необходимости.
    std::string kickMessage = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost" +
                          " KICK " + channel + " " + target + " :Kicked by operator\r\n";
    chan.broadcast(kickMessage);
    chan.removeMember(target_fd);
}


// Обработка команды INVITE: приглашение пользователя в канал.
void ChatServer::processInviteCommand(int client_fd, std::istringstream &iss) {
    std::string target, channel;
    iss >> target >> channel;

    Client &client = clients[client_fd];
    if (target.empty() || channel.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " INVITE :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channel[0] != '#' && channel[0] != '&') {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channels.find(channel) == channels.end()) {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    Channel &chan = channels[channel];

    if (!chan.isOperator(client_fd)) {
        std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
                               " " + channel + " :You're not channel operator\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    int target_fd = -1;
    for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.getNickname() == target) {
            target_fd = it->first;
            break;
        }
    }

    if (target_fd == -1) {
        std::string errorMsg = ":irc.localhost 401 " + client.getNickname() +
                               " " + target + " :No such nick/channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Добавляем пользователя в список приглашенных, если такая логика реализована.
    chan.inviteUser(target);

    std::string operPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
    std::string inviteMsg = operPrefix + " INVITE " + target + " " + channel + "\r\n";
    send(target_fd, inviteMsg.c_str(), inviteMsg.size(), 0);

    std::string replyMsg = ":irc.localhost 341 " + client.getNickname() + " " + target + " " + channel +
                           " :Invitation sent\r\n";
    send(client_fd, replyMsg.c_str(), replyMsg.size(), 0);
}


// Обработка команды TOPIC: получение или установка темы канала.
void ChatServer::processTopicCommand(int client_fd, std::istringstream &iss) {
    std::string channel;
    iss >> channel;

    Client &client = clients[client_fd];
    if (channel.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " TOPIC :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channel[0] != '#' && channel[0] != '&') {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channels.find(channel) == channels.end()) {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    Channel &chan = channels[channel];

    // Если дополнительных параметров нет – это запрос текущего топика.
    if (iss.peek() == EOF) {
        std::string currentTopic = chan.getTopic();
        if (currentTopic.empty()) {
            std::string response = ":irc.localhost 331 " + client.getNickname() +
                                   " " + channel + " :No topic is set\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
        } else {
            std::string response = ":irc.localhost 332 " + client.getNickname() +
                                   " " + channel + " :" + currentTopic + "\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
        }
        return;
    }

    // Читаем оставшийся текст как новую тему.
    std::string rest;
    std::getline(iss, rest);
    size_t firstChar = rest.find_first_not_of(" ");
    if (firstChar != std::string::npos) {
        rest = rest.substr(firstChar);
    }
    if (!rest.empty() && rest[0] == ':') {
        rest = rest.substr(1);
    }
    std::string topic = rest;

    // Если тема ограничена и клиент не оператор, отправляем ошибку.
    if (chan.isTopicRestricted() && !chan.isOperator(client_fd)) {
        std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
                               " " + channel + " :You're not channel operator\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Устанавливаем новую тему и рассылаем уведомление участникам канала.
    chan.setTopic(topic);
    std::string notification = ":" + client.getNickname() + "!" +
                               client.getUsername() + "@localhost TOPIC " +
                               channel + " :" + topic + "\r\n";
    chan.broadcast(notification);
}


// Обработка команды MODE: изменение режима канала.
// Читает имя канала, режим и дополнительные параметры, проверяет полномочия и применяет изменения.
void ChatServer::processModeCommand(int client_fd, std::istringstream &iss) {
    std::string channel, mode, param;
    iss >> channel >> mode >> param;

    Client &client = clients[client_fd];
    if (channel.empty() || mode.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " MODE :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channel[0] != '#' && channel[0] != '&') {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (channels.find(channel) == channels.end()) {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    if (!channels[channel].isOperator(client_fd)) {
        std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
                               " " + channel + " :You're not channel operator\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Применяем изменение режима.
    channels[channel].setMode(mode, param, client_fd);
}


// Обработка команды PART: выход клиента из канала.
// Извлекает имя канала и, возможно, сообщение о выходе, удаляет клиента из канала и рассылает уведомление.
void ChatServer::processPartCommand(int client_fd, std::istringstream &iss) {
    std::string channel;
    std::string partMessage;
    
    iss >> channel;               // Читаем имя канала.
    std::getline(iss, partMessage); // Читаем оставшийся текст как сообщение о выходе.
    
    // Удаляем ведущие пробелы и символы ':'.
    while (!partMessage.empty() && (partMessage[0] == ' ' || partMessage[0] == ':')) {
        partMessage.erase(0, 1);
    }
    
    Client &client = clients[client_fd];
    if (channel.empty()) {
        std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
                               " PART :Not enough parameters\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    
    if (channel[0] != '#' && channel[0] != '&') {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    
    if (channels.find(channel) == channels.end()) {
        std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
                               " " + channel + " :No such channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    
    Channel &chan = channels[channel];
    if (!chan.isMember(client_fd)) {
        std::string errorMsg = ":irc.localhost 442 " + client.getNickname() +
                               " " + channel + " :You're not on that channel\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }
    
    // Формируем уведомление о выходе.
    std::string senderPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
    std::string notification = senderPrefix + " PART " + channel;
    if (!partMessage.empty()) {
        notification += " :" + partMessage;
    }
    notification += "\r\n";
    
    // Удаляем клиента из канала и оповещаем участников.
    chan.removeMember(client_fd);
    chan.broadcast(notification);
}

// Обработка команды NOTICE: отправка уведомления получателю (либо в канал, либо лично).
// В отличие от PRIVMSG, сервер не должен генерировать автоматические ответы на NOTICE.
void ChatServer::processNoticeCommand(int client_fd, std::istringstream &iss) {
    std::string target, msg;
    // Читаем цель (канал или ник)
    iss >> target;
    // Читаем оставшуюся часть строки как сообщение
    std::getline(iss, msg);
    
    // Убираем ведущие пробелы и символ ':' из начала сообщения
    while (!msg.empty() && (msg[0] == ' ' || msg[0] == ':')) {
        msg.erase(0, 1);
    }
    
    // Отладочная информация (необязательно)
    std::cout << "NOTICE received - Target: '" << target << "', Message: '" << msg << "'" << std::endl;
    
    Client &client = clients[client_fd];
    
    // Если цель или сообщение отсутствуют, можно либо ничего не делать, либо отправить ошибку.
    // Обычно для NOTICE сервер не отправляет автоматических ответов, поэтому можно просто выйти.
    if (target.empty() || msg.empty()) {
        // Можно залогировать ошибку, но не отправлять ответ клиенту.
        std::cerr << "NOTICE: Not enough parameters from " << client.getNickname() << std::endl;
        return;
    }
    
    // Если цель начинается с '#' или '&' – это канал
    if (target[0] == '#' || target[0] == '&') {
        // Проверяем, существует ли канал
        if (channels.find(target) != channels.end()) {
            // (Опционально) Если требуется, можно проверить, является ли клиент участником канала.
            if (!channels[target].isMember(client_fd)) {
                // Обычно для NOTICE ошибки не отправляются, но можно записать в лог.
                std::cerr << "NOTICE: Client " << client.getNickname() << " not member of channel " << target << std::endl;
                return;
            }
            // Формируем префикс отправителя
            std::string senderPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
            // Формируем уведомление NOTICE и рассылаем его участникам канала
            std::string noticeMessage = senderPrefix + " NOTICE " + target + " :" + msg + "\r\n";
            channels[target].broadcast(noticeMessage);
        } else {
            // Канал не существует – можно залогировать ошибку
            std::cerr << "NOTICE: No such channel " << target << std::endl;
        }
    }
    // Иначе – цель считается ником для личного уведомления
    else {
        int recipientFd = getFdByNickname(target);
        if (recipientFd == -1) {
            std::cerr << "NOTICE: No such nick " << target << std::endl;
            return;
        } else {
            std::string senderPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
            std::string noticeMessage = senderPrefix + " NOTICE " + target + " :" + msg + "\r\n";
            send(recipientFd, noticeMessage.c_str(), noticeMessage.size(), 0);
        }
    }
}

// Обработка команды QUIT: клиент отправляет сообщение о выходе, и сервер
// уведомляет все каналы, в которых клиент участвует, о его отключении, затем закрывает соединение.
void ChatServer::processQuitCommand(int client_fd, std::istringstream &iss) {
    // Извлекаем оставшуюся часть строки как сообщение QUIT (например, причина ухода)
    std::string quitMessage;
    std::getline(iss, quitMessage);
    
    // Если есть лишние пробелы или двоеточие в начале, удаляем их
    if (!quitMessage.empty()) {
        if (quitMessage[0] == ' ')
            quitMessage.erase(0, 1);
        if (!quitMessage.empty() && quitMessage[0] == ':')
            quitMessage.erase(0, 1);
    }
    
    // Получаем объект клиента по его файловому дескриптору
    Client &client = clients[client_fd];
    
    // Формируем префикс отправителя в формате :<nick>!<username>@localhost
    std::string senderPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
    
    // Формируем строку уведомления QUIT. Если есть сообщение, добавляем его.
    std::string broadcastMessage = senderPrefix + " QUIT";
    if (!quitMessage.empty()) {
        broadcastMessage += " :" + quitMessage;
    }
    broadcastMessage += "\r\n";
    
    // Для каждого канала, где клиент является участником,
    // рассылаем уведомление об отключении и удаляем клиента из списка участников канала.
    // Используем итератор вместо range-based for (C++98 стиль).
    for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        Channel &chan = it->second;
        if (chan.isMember(client_fd)) {
            chan.broadcast(broadcastMessage);
            chan.removeMember(client_fd);
        }
    }
    
    // Выводим информацию о завершении соединения в консоль для отладки.
    std::cout << "Client " << client.getNickname() << " quit: " << quitMessage << std::endl;
    
    // Отключаем клиента: закрываем его сокет и удаляем его из списка активных клиентов.
    handleClientDisconnect(client_fd);
}
