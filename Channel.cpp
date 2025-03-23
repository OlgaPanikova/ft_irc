#include "Channel.hpp"

// Конструктор канала.
// Принимает имя канала (channelName) и инициализирует основные поля:
// - name: имя канала
// - operator_fd: файловый дескриптор оператора (по умолчанию -1, то есть оператор ещё не назначен)
// - inviteOnly: флаг, указывающий, что канал работает в режиме "только по приглашению" (по умолчанию false)
Channel::Channel(std::string channelName) : name(channelName), operator_fd(-1), inviteOnly(false) {}

Channel::Channel() : name(""), operator_fd(-1) {} 

// Добавляет клиента в канал.
// client_fd – файловый дескриптор клиента,
// nickname и username – соответствующие никнейм и имя пользователя, которые сохраняются для дальнейшей идентификации.
void Channel::addMember(int client_fd, const std::string& nickname, const std::string& username) {
    members.insert(client_fd); // Добавляем FD в множество участников
    memberNicknames[client_fd] = nickname; // Сохраняем никнейм, ассоциированный с FD
    memberUsernames[client_fd] = username; // Сохраняем юзернейм, ассоциированный с FD
}


// Возвращает строку, содержащую список участников канала.
// Если участник является оператором, перед его ником добавляется символ '@'.
std::string Channel::getMembersList() {
    std::string membersList;
    // Проходим по всем файловым дескрипторам участников канала.
    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (operators.find(*it) != operators.end()) {
            membersList += "@";  // Добавляем @ перед ником оператора
        }
        // Добавляем никнейм участника и пробел.
        membersList += memberNicknames[*it] + " ";
    }
    // Если список не пуст, удаляем последний лишний пробел.
    if (!membersList.empty()) {
        membersList.erase(membersList.size() - 1); // Убираем лишний пробел в конце
    }
    return membersList;
}

// Удаляет участника из канала по файловому дескриптору.
// Помимо удаления FD из множества участников, функция также удаляет ник из приглашённых,
// и, если удалённый участник был оператором, назначает нового оператора (первого в списке).
void Channel::removeMember(int client_fd) {
    members.erase(client_fd);
    
    // Получаем ник участника по FD, чтобы удалить его из списка приглашённых.
    std::string nickname;
    for (std::map<int, std::string>::const_iterator it = memberNicknames.begin(); it != memberNicknames.end(); ++it) {
        if (it->first == client_fd) {
            nickname = it->second;
            break;
        }
    }

    // Удаляем из списка приглашённых (если был приглашён).
    invitedUsers.erase(nickname);

    // Если удалённый клиент является оператором, назначаем нового оператора,
    // если в канале ещё есть участники.
    if (client_fd == operator_fd && !members.empty()) {
        operator_fd = *members.begin();  // ✅ Назначаем нового оператора
    }
}

// Назначает пользователя оператором, добавляя его FD в множество операторов.
void Channel::makeOperator(int client_fd) {
    operators.insert(client_fd);
}

// Проверяет, является ли клиент участником канала.
bool Channel::isMember(int client_fd) const {
        return members.find(client_fd) != members.end();
    }


// Отправляет сообщение всем участникам канала, кроме отправителя.
// Формируется корректное IRC-сообщение с префиксом, содержащим никнейм и юзернейм отправителя.
void Channel::sendMessageToChannel(const std::string& message, int sender_fd) {
    // Получаем никнейм и юзернейм отправителя.
    std::string sender_nickname = memberNicknames[sender_fd];
    std::string sender_username = memberUsernames[sender_fd]; // Получаем username отправителя

    // Формируем IRC-правильное сообщение, используя реальные nickname и username:
    std::string ircMessage = ":" + sender_nickname + "!" + sender_username + "@localhost PRIVMSG " + name + " :" + message + "\r\n";

    // Проходим по всем участникам канала и отправляем сообщение, исключая отправителя.
    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (*it != sender_fd) {  // Не отправляем отправителю
            send(*it, ircMessage.c_str(), ircMessage.length(), 0);
        }
    }
}


// Ищет и возвращает файловый дескриптор клиента по его никнейму.
// Если никнейм не найден, возвращает -1.
int Channel::getFdByNickname(const std::string &nickname) const {
    for (std::map<int, std::string>::const_iterator it = memberNicknames.begin(); it != memberNicknames.end(); ++it) {
        if (it->second == nickname) {
            return it->first;  // Возвращаем fd
        }
    }
    return -1;  // Если никнейм не найден
}



// Добавляет никнейм в список приглашённых пользователей.
void Channel::inviteUser(const std::string& nickname) {
    invitedUsers.insert(nickname);
}

// Устанавливает тему канала.
void Channel::setTopic(const std::string& newTopic) {
    topic = newTopic;
}

// Возвращает текущую тему канала.
std::string Channel::getTopic() const {
    return topic;
}

// Проверяет, установлен ли режим ограничения изменения темы (только операторы могут менять тему).
bool Channel::isTopicRestricted() const {
    return topicRestricted;
}

// Проверяет, работает ли канал в режиме "только по приглашению".
bool Channel::isInviteOnly() const {
    return inviteOnly;
}

// Проверяет, приглашён ли пользователь (по никнейму) в канал.
bool Channel::isInvited(const std::string& nickname) const {
    return invitedUsers.find(nickname) != invitedUsers.end();
}

// Возвращает никнейм, ассоциированный с данным файловым дескриптором.
// Если FD не найден, возвращает пустую строку.
std::string Channel::getNicknameForFd(int client_fd) {
    if (memberNicknames.find(client_fd) != memberNicknames.end()) {
        return memberNicknames[client_fd];
    }
    return "";
}


// Возвращает установленный ключ канала (если режим +k активен).
std::string Channel::getChannelKey() const {
    return channelKey;
}

// Проверяет, является ли клиент оператором канала.
bool Channel::isOperator(int client_fd) const {
    return operators.find(client_fd) != operators.end();
}


// Рассылает сообщение всем участникам канала.
// Принимает одно сообщение, которое отправляется через функцию send() для каждого FD в множестве members.
void Channel::broadcast(const std::string& message) {
    for (std::set<int>::iterator it = members.begin(); it != members.end(); ++it) {
        send(*it, message.c_str(), message.size(), 0);
    }
}


// Возвращает максимальное количество пользователей (лимит), установленное для канала.
int Channel::getUserLimit() const 
{ 
    return userLimit; 
}

// Возвращает текущее количество участников канала.
int Channel::getMemberCount() const 
{ 
    return members.size(); 
}


// Устанавливает режим канала (например, invite-only, topic-restricted, ключ доступа, операторы, лимит пользователей).
// Параметр client_fd используется для отправки сообщений об ошибках, если параметры некорректны.
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
        // Режим +k требует параметр (ключ)
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
        // Режим +o – назначение оператора
        int user_fd = getFdByNickname(param); // Функция поиска по нику
        if (user_fd == -1) {
            std::string errorMsg = ":irc.localhost 401 " + getNicknameForFd(client_fd) +
                                " " + param + " :No such nick/channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        operators.insert(user_fd);
        // Формируем MODE-сообщение для оповещения всех участников канала о выдаче прав
        std::string modeMsg = ":" + getNicknameForFd(client_fd) + "!" + getNicknameForFd(client_fd) +
                            "@localhost MODE " + name + " +o " + param + "\r\n";
        // Рассылаем сообщение всем участникам канала
        broadcast(modeMsg);
        logMessage = "User " + param + " is now an operator.";
    } else if (mode == "-o") {
        // Режим -o – снятие привилегий оператора.
        int user_fd = getFdByNickname(param);
        if (user_fd == -1) {
            std::string errorMsg = ":irc.localhost 401 " + getNicknameForFd(client_fd) +
                                   " " + param + " :No such nick/channel\r\n";
            send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
            return;
        }
        // Если оператор пытается снять привилегии другому оператору, запрещаем это действие
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
        // Режим +l – ограничение количества пользователей. Параметр должен быть числом.
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
        // Если режим не распознан, отправляем ERR_UNKNOWNMODE (код 472)
        std::string errorMsg = ":irc.localhost 472 " + getNicknameForFd(client_fd) +
                               " " + mode + " :is unknown mode char for " + name + "\r\n";
        send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Логирование изменения режима
    if (!logMessage.empty()) {
        std::cout << "Setting mode " << mode << " on channel " << name << ": " << logMessage << std::endl;
    }
}








// bool Channel::isMemberByNick(const std::string& nickname) const {
//     return getFdByNickname(nickname) != -1;
// }


// void Channel::kickUser(const std::string& nickname) {
//     int user_fd = getFdByNickname(nickname);
//     if (user_fd != -1) {
//         removeMember(user_fd);
//         send(user_fd, "You have been kicked from the channel.\n", 39, 0);
//     }
// }