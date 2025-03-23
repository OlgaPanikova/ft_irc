#include "ChatServer.hpp"

// Инициализирует сервер, создаёт и настраивает сокет, привязывает его к указанному порту и начинает прослушивание входящих соединений.
ChatServer::ChatServer(int port, const std::string &password) 
        : serverPassword(password), serverPort(port) {
    // Создание TCP-сокета.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::perror("Socket failed");
        exit(1);
    }

     // Установка сокета в неблокирующий режим.
    setNonBlocking(server_fd);

    // Разрешение переиспользования адреса (полезно при перезапуске сервера).
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Настройка адресной структуры сервера.
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Привязка ко всем доступным интерфейсам
    server_addr.sin_port = htons(serverPort); // Порт (приводится к сетевому порядку байт)

    // Привязка сокета к адресу и порту.
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Начало прослушивания входящих соединений с максимальным числом клиентов MAX_CLIENTS.
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(1);
    }

    // Добавление серверного сокета в вектор файловых дескрипторов для работы с poll().
    pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);

    // Вывод сообщения о запуске сервера.
    std::cout << "Server started on port " << serverPort << std::endl;
    std::cout << serverPassword << std::endl;
}

// неблокирующий режим 
void ChatServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
    }
}

ChatServer::~ChatServer() {
    for (size_t i = 0; i < fds.size(); i++) {
        close(fds[i].fd);
    }
}

// Основной цикл работы сервера, который постоянно отслеживает события на всех сокетах.
void ChatServer::run() {
    // Бесконечный цикл, который продолжает работу сервера, пока он не остановится или не произойдет ошибка.
    while (true) {
        // Вызов функции poll для ожидания событий на всех файловых дескрипторах, хранящихся в векторе fds.
        // &fds[0] — указатель на первый элемент массива структур pollfd.
        // fds.size() — количество дескрипторов, за которыми следим.
        // -1 — бесконечное ожидание (poll будет ждать, пока не произойдет какое-либо событие).
        int ret = poll(&fds[0], fds.size(), -1);
        if (ret < 0) {
            perror("Poll error");
            break;
        }

        // Проходим по всем файловым дескрипторам, чтобы проверить, на каких из них произошло событие.
        for (size_t i = 0; i < fds.size(); i++) {
            // Проверяем, установлен ли флаг POLLIN в поле revents (означает, что на сокете появились данные для чтения).
            if (fds[i].revents & POLLIN) {
                // Если событие произошло на серверном сокете (сервер ожидает новое подключение).
                if (fds[i].fd == server_fd) {
                    handleNewConnection(); // Обрабатываем новое входящее соединение.
                } else {
                    // Если событие произошло на клиентском сокете, обрабатываем входящее сообщение от клиента.
                    handleClientMessage(fds[i].fd);
                }
            }
        }
    }
}


// Функция для обработки нового входящего соединения.
// При вызове функции происходит принятие нового клиента, его настройка и регистрация в системе.
void ChatServer::handleNewConnection() {
    // Создаем структуру для хранения адреса клиента.
    struct sockaddr_in client_addr;

    // Задаем размер структуры адреса клиента.
    socklen_t client_len = sizeof(client_addr);

    // Функция accept принимает новое входящее соединение на серверном сокете (server_fd)
    // и возвращает файловый дескриптор для общения с этим клиентом.
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Accept failed");
        return;
    }

    // Переводим новый клиентский сокет в неблокирующий режим.
    // Это необходимо, чтобы операции ввода/вывода не блокировали сервер при ожидании данных.
    setNonBlocking(client_fd);

    // Выводим в консоль сообщение о новом подключении, показывая IP-адрес клиента.
    std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;

    // Отправляем запрос на ввод пароля
    std::string passwordPrompt = ":irc.localhost NOTICE * :Please enter the password using PASS <password>.\r\n";
    send(client_fd, passwordPrompt.c_str(), passwordPrompt.size(), 0);

    // Создаем новую структуру pollfd для отслеживания событий на клиентском сокете.
    pollfd new_pollfd;
    new_pollfd.fd = client_fd; // Устанавливаем файловый дескриптор нового клиента.
    new_pollfd.events = POLLIN; // Указываем, что нас интересует событие готовности к чтению.
    new_pollfd.revents = 0; // Изначально флаг событий сбрасывается.

    // Добавляем структуру с новым клиентским дескриптором в вектор fds,
    // который используется для отслеживания событий с помощью poll().
    fds.push_back(new_pollfd);

    // Создаем объект Client для нового подключения.
    // Конструктор Client принимает файловый дескриптор, который будет использоваться для связи с клиентом.
    Client newClient(client_fd);

    // Устанавливаем, что клиент еще не прошел аутентификацию (пароль не введен).
    newClient.setAuthenticated(false);

    // Добавляем нового клиента в контейнер клиентов,
    // используя файловый дескриптор в качестве уникального идентификатора.
    clients[client_fd] = newClient;
}

// Основная функция для обработки входящих сообщений от клиента.
void ChatServer::handleClientMessage(int client_fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    // Чтение данных из сокета клиента
    int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    // Если данных нет или произошла ошибка при чтении
    if (bytes_read <= 0) {
        // Если ошибка вызвана временной недоступностью данных – просто возвращаем
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        // Иначе обрабатываем отключение клиента
        handleClientDisconnect(client_fd);
        return;
    }

    // Получаем ссылку на объект клиента по его файловому дескриптору
    Client &client = clients[client_fd];
    // Преобразуем полученные данные в строку и добавляем их в буфер клиента
    std::string data(buffer, bytes_read);
    client.appendToBuffer(data);

    // Извлекаем из буфера полные сообщения, разделённые символом '\n'
    while (true) {
        size_t pos = client.getBuffer().find('\n');
        if (pos == std::string::npos) {
            break;  // Если полного сообщения нет – выходим из цикла
        }
        // Извлекаем сообщение до символа новой строки
        std::string message = client.getBuffer().substr(0, pos);
        // Удаляем обработанную часть из буфера
        client.clearBuffer(pos);
        // Если сообщение заканчивается символом '\r', удаляем его
        if (!message.empty() && message[message.size() - 1] == '\r') {
            message.erase(message.size() - 1);
        }
        // Обрабатываем извлечённое сообщение
        processCompleteMessage(client_fd, message);
    }
}

// Функция для обработки отключения клиента.
// Закрывает сокет, удаляет его из списка отслеживаемых дескрипторов и убирает клиента из контейнера.
void ChatServer::handleClientDisconnect(int client_fd) {
    std::cout << "Client disconnected (fd=" << client_fd << ")\n";
    close(client_fd);
    // Удаляем файловый дескриптор клиента из вектора fds
    for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
        if (it->fd == client_fd) {
            fds.erase(it);
            break;
        }
    }
    // Удаляем клиента из контейнера клиентов
    clients.erase(client_fd);
}

// Функция для обработки одного полного сообщения, полученного от клиента.
// Здесь происходит разбор команды, обработка специальных команд (PING, PASS, NICK, USER),
// отправка приветственного сообщения и вызов обработки остальных команд.
void ChatServer::processCompleteMessage(int client_fd, const std::string &message) {
    // Получаем объект клиента по его файловому дескриптору
    Client &client = clients[client_fd];

    // Создаем поток для разбора сообщения
    std::istringstream iss(message);
    std::string command;
    // Считываем первую часть сообщения как команду
    iss >> command;

    // Считываем оставшуюся часть строки как параметры команды
    std::string param;
    std::getline(iss, param);
    // Удаляем ведущий пробел, если он присутствует
    if (!param.empty() && param[0] == ' ') {
        param.erase(0, 1);
    }

    // Удаляем символы '/' и '\' в начале команды
    while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
        command.erase(0, 1);
    }
    // Приводим команду к верхнему регистру для единообразия
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    // Обработка команды PING – отвечает PONG и завершается обработка текущего сообщения
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

    // Если клиент еще не аутентифицирован, разрешаем выполнение только команды PASS
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

    // Обработка команды NICK – установка никнейма клиента
    if (command == "NICK") {
        if (param.empty()) {
            std::string response = ":irc.localhost 431 * :No nickname given\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }
        client.setNickname(param);
    }

    // Обработка команды USER – регистрация пользователя
    if (command == "USER") {
        handleUSERCommand(client_fd, param, client);
    }

    // Если клиент полностью зарегистрирован (заданы NICK и USER) и приветствие еще не отправлено,
    // отправляем сообщение Welcome и информацию MOTD.
    if (!client.hasSentWelcome() && client.hasNickname() && client.hasUsername()) {
        client.setSentWelcome(true);
        std::string welcomeMsg = ":irc.localhost 001 " + client.getNickname() + " :Welcome to the IRC server!\r\n";
        std::string motdStart = ":irc.localhost 375 " + client.getNickname() + " :- IRC Message of the Day -\r\n";
        std::string motdEnd = ":irc.localhost 376 " + client.getNickname() + " :End of /MOTD command.\r\n";
        send(client_fd, welcomeMsg.c_str(), welcomeMsg.size(), 0);
        send(client_fd, motdStart.c_str(), motdStart.size(), 0);
        send(client_fd, motdEnd.c_str(), motdEnd.size(), 0);
    }

    // Для остальных команд (кроме PASS, NICK, USER) требуется, чтобы клиент был полностью зарегистрирован.
    if (command != "PASS" && command != "NICK" && command != "USER") {
        if (!client.hasNickname() || !client.hasUsername()) {
            std::string response = ":irc.localhost 451 * :You have not registered\r\n";
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }
    }

    // Обработка остальных команд:
    // Если команда известна (isCommand возвращает true), вызывается функция processCommand.
    // Иначе клиенту отправляется сообщение об ошибке (Unknown command).
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
        // Если токен начинается с ':' — считаем, что это начало realname
        if (token[0] == ':') {
            // Получаем остаток строки (без ведущих пробелов)
            std::string rest;
            std::getline(iss, rest);
            if (!rest.empty()) {
                tokens.back() += rest; // добавляем остаток к последнему токену
            }
            break;
        }
    }
    return tokens;
}

void ChatServer::handleUSERCommand(int client_fd, const std::string &param, Client &client) {
    printf("%s\n", param.c_str());
    std::vector<std::string> tokens = splitParams(param);
    // Проверяем, что получено минимум 4 параметра
    if (tokens.size() < 4) {
        std::string response = ":irc.localhost 461 * USER :Not enough parameters\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }
    
    // Извлекаем имя пользователя и realname.
    std::string username = tokens[0];
    // Второй и третий параметр можно сохранить, если потребуется, например:
    std::string mode = tokens[1];
    std::string unused = tokens[2];
    // Четвёртый и последующие токены формируют realname. Если realname начинается с ':', можно удалить этот символ.
    std::string realname = tokens[3];
    if (!realname.empty() && realname[0] == ':') {
        realname.erase(0, 1);
    }
    
    // Если в токенах было больше 4 частей, добавить их к realname (с пробелами)
    for (size_t i = 4; i < tokens.size(); i++) {
        realname += " " + tokens[i];
    }
    
    // Пример: можно добавить дополнительную проверку на корректность username и realname
    if (username.empty()) {
        std::string response = ":irc.localhost 461 * USER :Invalid username\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }
    
    client.setUsername(username);
    
    // Дополнительно можно сохранить mode и unused, если они важны для дальнейшей логики
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

// Основная функция обработки команд, полученных от клиента.
// Она извлекает команду из сообщения, нормализует её (удаляет ведущие '/' или '\' и приводит к верхнему регистру)
// и делегирует обработку конкретной команды соответствующему вспомогательному методу.
void ChatServer::processCommand(int client_fd, const std::string &message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    // Убираем ведущие символы '/' или '\' из команды.
    while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
        command.erase(0, 1);
    }

    // Приводим команду к верхнему регистру для единообразной обработки.
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "NICK" || command == "USER" || command == "PASS") {
        return;
    }
    // Делегируем обработку команды соответствующему методу.
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
        // Если команда не распознана, отправляем ошибку "Unknown command".
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
    return -1;  // Клиент с таким ником не найден
}







// void ChatServer::handleClientMessage(int client_fd) {
//     char buffer[BUFFER_SIZE];
//     memset(buffer, 0, BUFFER_SIZE);
//     int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

//     if (bytes_read <= 0) {
//         if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//             return;
//         }
//         std::cout << "Client disconnected (fd=" << client_fd << ")\n";
//         close(client_fd);
//         for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
//             if (it->fd == client_fd) {
//                 fds.erase(it);
//                 break;
//             }
//         }
//         clients.erase(client_fd);
//         return;
//     }

//     Client &client = clients[client_fd];
//     std::string data(buffer, bytes_read);
//     client.appendToBuffer(data);

//     while (true) {
//         size_t pos = client.getBuffer().find('\n');
//         if (pos == std::string::npos) {
//             break;
//         }

//         std::string message = client.getBuffer().substr(0, pos);
//         client.clearBuffer(pos);
//         if (!message.empty() && message.back() == '\r') {
//             message.pop_back();
//         }

//         std::istringstream iss(message);
//         std::string command;
//         iss >> command;

//         // Считываем всю оставшуюся строку как параметры:
//         std::string param;
//         std::getline(iss, param);

//         // Удаляем возможный ведущий пробел
//         if (!param.empty() && param[0] == ' ') {
//             param.erase(0, 1);
//         }

//         // Удаляем `/` и `\` в начале команды
//         while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
//             command.erase(0, 1);
//         }
//         std::transform(command.begin(), command.end(), command.begin(), ::toupper);

//         // === PING ===
//         if (command == "PING") {
//             std::string token = param;
//             std::string rest;
//             std::getline(iss, rest);
//             if (!rest.empty()) token += rest;

//             if (token.empty()) token = ":irc.localhost";
//             if (token[0] != ':') token = ":" + token;

//             std::string response = "PONG " + token + "\r\n";
//             send(client_fd, response.c_str(), response.size(), 0);
//             return;
//         }

//         // === PASS ===
//         if (!client.isAuthenticated()) {
//             if (command == "PASS") {
//                 if (param.empty()) {
//                     std::string response = ":irc.localhost 461 * PASS :Not enough parameters.\r\n";
//                     send(client_fd, response.c_str(), response.size(), 0);
//                     return;
//                 }
//                 if (param == serverPassword) {
//                     client.setAuthenticated(true);
//                     std::string response = ":irc.localhost NOTICE * :Password accepted. Please enter NICK and USER.\r\n";
//                     send(client_fd, response.c_str(), response.size(), 0);
//                 } else {
//                     std::string response = ":irc.localhost 464 * :Incorrect password.\r\n";
//                     send(client_fd, response.c_str(), response.size(), 0);
//                     close(client_fd);
//                     clients.erase(client_fd);
//                     return;
//                 }
//             } else {
//                 std::string response = ":irc.localhost NOTICE * :Please enter the password using PASS <password>\r\n";
//                 send(client_fd, response.c_str(), response.size(), 0);
//             }
//             continue;
//         }

//         // === NICK (можно без полной аутентификации) ===
//         if (command == "NICK") {
//             if (param.empty()) {
//                 std::string response = ":irc.localhost 431 * :No nickname given\r\n";
//                 send(client_fd, response.c_str(), response.size(), 0);
//                 return;
//             }
//             client.setNickname(param);
//         }

//         // === USER (можно без полной аутентификации) ===
//         if (command == "USER") {
//             handleUSERCommand(client_fd, param, client);
//         }

//         // ✅ После `NICK` и `USER`, если регистрация завершена — отправляем Welcome
//         if (!client.hasSentWelcome() && client.hasNickname() && client.hasUsername()) {
//             client.setSentWelcome(true);
//             std::string welcomeMsg = ":irc.localhost 001 " + client.getNickname() + " :Welcome to the IRC server!\r\n";
//             std::string motdStart = ":irc.localhost 375 " + client.getNickname() + " :- IRC Message of the Day -\r\n";
//             std::string motdEnd = ":irc.localhost 376 " + client.getNickname() + " :End of /MOTD command.\r\n";

//             send(client_fd, welcomeMsg.c_str(), welcomeMsg.size(), 0);
//             send(client_fd, motdStart.c_str(), motdStart.size(), 0);
//             send(client_fd, motdEnd.c_str(), motdEnd.size(), 0);
//         }

//         // === Проверка, что все остальные команды можно выполнять только после `NICK` и `USER` ===
//         if (command != "PASS" && command != "NICK" && command != "USER") {
//             if (!client.hasNickname() || !client.hasUsername()) {
//                 std::string response = ":irc.localhost 451 * :You have not registered\r\n";
//                 send(client_fd, response.c_str(), response.size(), 0);
//                 return;
//             }
//         }

//         // === Обработка остальных команд ===
//         if (isCommand(command)) {
//             processCommand(client_fd, message);
//         } else {
//             std::string response = ":irc.localhost 421 * " + command + " :Unknown command\r\n";
//             send(client_fd, response.c_str(), response.size(), 0);
//         }
//     }
// }


// void ChatServer::processCommand(int client_fd, const std::string &message) {
//     std::istringstream iss(message);
//     std::string command;
//     iss >> command;

//      while (!command.empty() && (command[0] == '/' || command[0] == '\\')) {
//         command.erase(0, 1);
//     }

//     // ✅ Приводим команду к верхнему регистру
//     std::transform(command.begin(), command.end(), command.begin(), ::toupper);

// if (command == "JOIN") {
//     std::string param, key;
//     iss >> param >> key;  // ✅ Читаем возможный ключ (пароль)

//     if (param.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + clients[client_fd].getNickname() + " JOIN :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     if (param[0] != '#') {
//         param = "#" + param;
//     }

//     bool isNewChannel = (channels.find(param) == channels.end());

//     if (isNewChannel) {
//         channels.insert(std::make_pair(param, Channel(param)));
//         std::cout << "Created new channel: " << param << std::endl;
//     } else {
//         Channel &chan = channels[param];

//         // ✅ Проверяем invite-only режим
//         if (chan.isInviteOnly() && !chan.isInvited(clients[client_fd].getNickname())) {
//             std::string errorMsg = ":irc.localhost 473 " + clients[client_fd].getNickname() + " " + param + " :Cannot join: Invite-only channel\r\n";
//             send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//             return;
//         }

//         if (chan.getUserLimit() > 0 && chan.getMemberCount() >= chan.getUserLimit()) {
//             std::string errorMsg = ":irc.localhost 471 " + clients[client_fd].getNickname() + " " + param + " :Cannot join: Channel is full\r\n";
//             send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//             return;
//         }

//         // ✅ Проверяем пароль канала (если установлен)
//         if (!chan.getChannelKey().empty() && chan.getChannelKey() != key) {
//             std::string errorMsg = ":irc.localhost 475 " + clients[client_fd].getNickname() + " " + param + " :Cannot join: Incorrect channel key\r\n";
//             send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//             return;
//         }
//     }

//     std::string nickname = clients[client_fd].getNickname();
//     if (nickname.empty()) {
//         send(client_fd, "You must set a nickname before joining a channel.\r\n", 50, 0);
//         return;
//     }

//     std::string username = clients[client_fd].getUsername();
//     if (username.empty()) {
//         send(client_fd, "You must set a username before joining a channel.\r\n", 50, 0);
//         return;
//     }

//     channels[param].addMember(client_fd, nickname, username);
//     clients[client_fd].setCurrentChannel(param);

//     if (isNewChannel) {
//         channels[param].makeOperator(client_fd);
//         std::string response = "You are now the channel operator.\r\n";
//         send(client_fd, response.c_str(), response.size(), 0);
//     }

//     std::string response = "Joined " + param + "\n";
//     send(client_fd, response.c_str(), response.size(), 0);
//     std::cout << "User " << client_fd << " joined channel: " << param << std::endl;
//         // ✅ Отправляем сообщение о входе в канал
//     std::string joinMsg = ":" + nickname + "!" + username + "@localhost JOIN " + param + "\r\n";
//     channels[param].broadcast(joinMsg);

//     // ✅ Отправляем текущую тему канала
//     std::string topic = channels[param].getTopic();
//     std::string topicMsg;
//     if (!topic.empty()) {
//         topicMsg = ":irc.localhost 332 " + nickname + " " + param + " :" + topic + "\r\n";
//     } else {
//         topicMsg = ":irc.localhost 331 " + nickname + " " + param + " :No topic is set\r\n";
//     }
//     send(client_fd, topicMsg.c_str(), topicMsg.size(), 0);

//     // ✅ Отправляем список пользователей в канале
//     std::string namesList = ":irc.localhost 353 " + nickname + " = " + param + " :";
//     namesList += channels[param].getMembersList(); // Метод должен вернуть список участников
//     namesList += "\r\n";
//     send(client_fd, namesList.c_str(), namesList.size(), 0);

//     // ✅ Отправляем конец списка пользователей
//     std::string endNames = ":irc.localhost 366 " + nickname + " " + param + " :End of /NAMES list\r\n";
//     send(client_fd, endNames.c_str(), endNames.size(), 0);
// }
    


// else if (command == "PRIVMSG") {
//     std::string target, msg;
//     iss >> target;  // Читаем цель: либо канал, либо ник

//     // Забираем остаток строки как сообщение
//     std::getline(iss, msg);

//     // Выводим отладочную информацию
//     std::cout << "PRIVMSG received - Target: '" << target << "', Message: '" << msg << "'" << std::endl;

//     // Убираем ведущие пробелы и двоеточие перед сообщением
//     while (!msg.empty() && (msg[0] == ' ' || msg[0] == ':')) {
//         msg.erase(0, 1);
//     }

//     Client &client = clients[client_fd];
//     // Проверяем, введены ли оба параметра
//     if (target.empty() || msg.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " PRIVMSG :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Если цель начинается с '#' или '&', это канал
//     if (target[0] == '#' || target[0] == '&') {
//         // Проверяем, существует ли канал
//         if (channels.find(target) != channels.end()) {
//             // Проверяем, является ли клиент участником канала
//             if (!channels[target].isMember(client_fd)) {
//                 std::string errorMsg = ":irc.localhost 404 " + client.getNickname() +
//                                        " " + target + " :Cannot send to channel\r\n";
//                 send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//                 return;
//             }
//             channels[target].sendMessageToChannel(msg, client_fd);
//         } else {
//             std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                    " " + target + " :No such channel\r\n";
//             send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         }
//     }
//     // Иначе, предполагаем, что это ник (личное сообщение)
//     else {
//         // Предполагается, что есть функция поиска клиента по нику:
//         int recipientFd = getFdByNickname(target);
//         if (recipientFd == -1) {
//             std::string errorMsg = ":irc.localhost 401 " + client.getNickname() +
//                                    " " + target + " :No such nick/channel\r\n";
//             send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         } else {
//             std::string senderPrefix = ":" + client.getNickname() + "!" +
//                                        client.getUsername() + "@localhost";
//             std::string messageToSend = senderPrefix + " PRIVMSG " + target +
//                                         " :" + msg + "\r\n";
//             send(recipientFd, messageToSend.c_str(), messageToSend.size(), 0);
//         }
//     }
// }


// else if (command == "KICK") {
//     std::string channel, target;
//     iss >> channel >> target;

//     Client &client = clients[client_fd];
//     // Проверяем наличие параметров
//     if (channel.empty() || target.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " KICK :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Если канал не начинается с '#' или '&', считаем, что это ошибка
//     if (channel[0] != '#' && channel[0] != '&') {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, существует ли канал
//     if (channels.find(channel) == channels.end()) {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     Channel &chan = channels[channel];

//     // Проверяем, является ли отправитель оператором канала
//     if (!chan.isOperator(client_fd)) {
//         std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
//                                " " + channel + " :You're not channel operator\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Поиск целевого пользователя (по нику) среди клиентов
//     int target_fd = -1;
//     for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
//         if (it->second.getNickname() == target) {
//             target_fd = it->first;
//             break;
//         }
//     }

//     // Если целевой пользователь не найден или не является участником канала
//     if (target_fd == -1 || !chan.isMember(target_fd)) {
//         std::string errorMsg = ":irc.localhost 441 " + client.getNickname() +
//                                " " + target + " " + channel +
//                                " :They aren't on that channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Формируем сообщение о кике.
//     // Можно добавить комментарий, например, пустой или указать ник цели.
//     std::string comment = target; // или любой другой комментарий
//     std::string kickMessage = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost" +
//                               " KICK " + channel + " " + target + " :" + comment + "\r\n";

//     // Отправляем сообщение всем участникам канала
//     chan.broadcast(kickMessage);

//     // Удаляем пользователя из канала
//     chan.removeMember(target_fd);
// }


// else if (command == "INVITE") {
//     // Читаем параметры в правильном порядке: сначала ник, затем канал
//     std::string target, channel;
//     iss >> target >> channel;

//      Client &client = clients[client_fd];
//     // Проверка параметров
//     if (target.empty() || channel.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " INVITE :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Если канал не начинается с '#' или '&', считаем, что канал неверный
//     if (channel[0] != '#' && channel[0] != '&') {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, существует ли канал
//     if (channels.find(channel) == channels.end()) {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     Channel &chan = channels[channel];

//     // Проверяем, является ли отправитель оператором канала
//     if (!chan.isOperator(client_fd)) {
//         std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
//                                " " + channel + " :You're not channel operator\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, существует ли приглашённый пользователь
//     int target_fd = -1;
//     for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it) {
//         if (it->second.getNickname() == target) {
//             target_fd = it->first;
//             break;
//         }
//     }

//     if (target_fd == -1) {
//         std::string errorMsg = ":irc.localhost 401 " + client.getNickname() +
//                                " " + target + " :No such nick/channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Добавляем пользователя в список приглашённых (если такая логика реализована)
//     chan.inviteUser(target);

//     // Формируем префикс оператора
//     std::string operPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
    
//     // Отправляем приглашение приглашённому пользователю в формате:
//     // :<operNick>!<operUser>@localhost INVITE <target> <channel>\r\n
//     std::string inviteMsg = operPrefix + " INVITE " + target + " " + channel + "\r\n";
//     send(target_fd, inviteMsg.c_str(), inviteMsg.size(), 0);

//     // Отправляем подтверждение оператору (код 341)
//     std::string replyMsg = ":irc.localhost 341 " + client.getNickname() + " " + target + " " + channel + " :Invitation sent\r\n";
//     send(client_fd, replyMsg.c_str(), replyMsg.size(), 0);
// }

// else if (command == "TOPIC") {
//     std::string channel, topic;
//     iss >> channel;

//     Client &client = clients[client_fd];
//     if (channel.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " TOPIC :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Если канал не начинается с '#' или '&', считаем, что это неверное имя канала
//     if (channel[0] != '#' && channel[0] != '&') {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, существует ли канал
//     if (channels.find(channel) == channels.end()) {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     Channel &chan = channels[channel];

//     // Если после команды TOPIC ничего не передано – это запрос текущего топика
//     if (iss.peek() == EOF) {
//         std::string currentTopic = chan.getTopic();
//         if (currentTopic.empty()) {
//             // RPL_NOTOPIC, код 331
//             std::string response = ":irc.localhost 331 " + client.getNickname() +
//                                    " " + channel + " :No topic is set\r\n";
//             send(client_fd, response.c_str(), response.size(), 0);
//         } else {
//             // RPL_TOPIC, код 332
//             std::string response = ":irc.localhost 332 " + client.getNickname() +
//                                    " " + channel + " :" + currentTopic + "\r\n";
//             send(client_fd, response.c_str(), response.size(), 0);
//         }
//         return;
//     }

//     // Читаем весь оставшийся текст, включая возможное двоеточие
//     std::string rest;
//     std::getline(iss, rest);

//     // Убираем ведущие пробелы
//     size_t firstChar = rest.find_first_not_of(" ");
//     if (firstChar != std::string::npos) {
//         rest = rest.substr(firstChar);
//     }

//     // Если первый символ — двоеточие, убираем его
//     if (!rest.empty() && rest[0] == ':') {
//         rest = rest.substr(1);
//     }
//     topic = rest;

//     // Проверяем, может ли этот пользователь менять топик
//     if (chan.isTopicRestricted() && !chan.isOperator(client_fd)) {
//         std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
//                                " " + channel + " :You're not channel operator\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Устанавливаем новый топик
//     chan.setTopic(topic);
//     // После смены топика сервер должен расслать уведомление всем участникам канала.
//     // Формат сообщения:
//     // :<nick>!<username>@localhost TOPIC <channel> :<topic>\r\n
//     std::string notification = ":" + client.getNickname() + "!" +
//                                client.getUsername() + "@localhost TOPIC " +
//                                channel + " :" + topic + "\r\n";
//     chan.broadcast(notification);
// }


// else if (command == "MODE") {
//     std::string channel, mode, param;
//     iss >> channel >> mode >> param;

//     Client &client = clients[client_fd];
//     // Проверка параметров (ERR_NEEDMOREPARAMS – 461)
//     if (channel.empty() || mode.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " MODE :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Если имя канала не начинается с '#' или '&', считаем его некорректным (ERR_NOSUCHCHANNEL – 403)
//     if (channel[0] != '#' && channel[0] != '&') {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, существует ли канал (ERR_NOSUCHCHANNEL – 403)
//     if (channels.find(channel) == channels.end()) {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Проверяем, является ли вызывающий оператором (ERR_CHANOPRIVSNEEDED – 482)
//     if (!channels[channel].isOperator(client_fd)) {
//         std::string errorMsg = ":irc.localhost 482 " + client.getNickname() +
//                                " " + channel + " :You're not channel operator\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }

//     // Обрабатываем изменение режима
//     channels[channel].setMode(mode, param, client_fd);
// }

// else if (command == "PART") {
//     std::string channel;
//     std::string partMessage;
    
//     // Читаем имя канала
//     iss >> channel;
//     // Читаем оставшийся текст как сообщение (если есть)
//     std::getline(iss, partMessage);
    
//     // Убираем ведущие пробелы и двоеточие, если оно присутствует
//     while (!partMessage.empty() && (partMessage[0] == ' ' || partMessage[0] == ':')) {
//         partMessage.erase(0, 1);
//     }
//     Client &client = clients[client_fd];
//     // Если имя канала не указано, возвращаем ошибку: ERR_NEEDMOREPARAMS (461)
//     if (channel.empty()) {
//         std::string errorMsg = ":irc.localhost 461 " + client.getNickname() +
//                                " PART :Not enough parameters\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }
    
//     // Если имя канала не начинается с '#' или '&', считаем, что такого канала не существует (ERR_NOSUCHCHANNEL, 403)
//     if (channel[0] != '#' && channel[0] != '&') {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }
    
//     // Проверяем, существует ли канал
//     if (channels.find(channel) == channels.end()) {
//         std::string errorMsg = ":irc.localhost 403 " + client.getNickname() +
//                                " " + channel + " :No such channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }
    
//     Channel &chan = channels[channel];
    
//     // Проверяем, является ли клиент участником канала (ERR_NOTONCHANNEL, 442)
//     if (!chan.isMember(client_fd)) {
//         std::string errorMsg = ":irc.localhost 442 " + client.getNickname() +
//                                " " + channel + " :You're not on that channel\r\n";
//         send(client_fd, errorMsg.c_str(), errorMsg.size(), 0);
//         return;
//     }
    
//     // Формируем префикс отправителя: :<nick>!<username>@localhost
//     std::string senderPrefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
    
//     // Формируем уведомление о выходе, согласно стандарту IRC:
//     // :<nick>!<username>@localhost PART <channel> :<part message>
//     std::string notification = senderPrefix + " PART " + channel;
//     if (!partMessage.empty()) {
//         notification += " :" + partMessage;
//     }
//     notification += "\r\n";
    
//     // Удаляем клиента из канала
//     chan.removeMember(client_fd);
    
//     // Рассылаем уведомление всем участникам канала
//     chan.broadcast(notification);
// }
// }



// void ChatServer::sendMessageToAllClients(int sender_fd, const std::string &message) {
//     Client &client = clients[sender_fd];
//     std::string currentChannel = client.getCurrentChannel();

//     if (currentChannel.empty() || channels.find(currentChannel) == channels.end()) {
//         send(sender_fd, "You are not in a channel.\n", 26, 0);
//         return;
//     }

//     channels[currentChannel].sendMessageToChannel(client.getNickname() + ": " + message + "\n", sender_fd);
// }


