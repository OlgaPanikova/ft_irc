#include "Client.hpp"

// Конструктор клиента, принимает файловый дескриптор сокета.
// Инициализирует внутренние поля: флаг аутентификации, наличие никнейма, юзернейма и флаг отправки welcome-сообщения.
Client::Client(int fd) {
    this->fd = fd; // Сохраняем файловый дескриптор для данного клиента.
    this->authenticated = false; // По умолчанию клиент не аутентифицирован.
    this->hasNick = false; // Никнейм еще не установлен.
    this->hasUser = false; // Юзернейм еще не установлен.
    this->welcomeSent = false; // Welcome-сообщение еще не отправлено.
}

// Конструктор по умолчанию.
// Используется, если файловый дескриптор неизвестен или не требуется немедленная инициализация.
Client::Client() {
    this->fd = -1;
    this->authenticated = false;
    this->hasNick = false;
    this->hasUser = false;
}

// Метод возвращает, аутентифицирован ли клиент.
// Используется для проверки, прошел ли клиент аутентификацию.
bool Client::isAuthenticated() const {
    return this->authenticated;
}

// Метод устанавливает флаг аутентификации для клиента.
void Client::setAuthenticated(bool value) {
    this->authenticated = value;
}

// Метод проверяет, установлен ли никнейм у клиента.
bool Client::hasNickname() const {
    return hasNick;
}

// Метод проверяет, установлен ли юзернейм у клиента.
bool Client::hasUsername() const {
    return hasUser;
}

// Метод установки никнейма.
// Если никнейм пустой, отправляется сообщение об ошибке на сокет клиента.
void Client::setNickname(const std::string &nickname) {
    if (nickname.empty()) {
        send(fd, "ERROR: Nickname cannot be empty\n", 30, 0);
        return;
    }
    this->nickname = nickname;
    this->hasNick = true;
    std::cout << "Client " << fd << " set nickname to " << nickname << std::endl;
}

// Метод установки юзернейма.
// Если юзернейм пустой, отправляется сообщение об ошибке.
// При установке юзернейма клиент считается аутентифицированным.
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

// Метод возвращает файловый дескриптор клиента.
int Client::getFd() const { 
    return fd; 
}

// Метод возвращает установленный никнейм клиента.
std::string Client::getNickname() const {
    return nickname; 
}

// Метод возвращает установленный юзернейм клиента.
std::string Client::getUsername() const {
    return username; 
}

// Метод добавляет полученные данные в буфер клиента.
// Буфер используется для накопления входящих данных до получения полного сообщения.
void Client::appendToBuffer(const std::string &data) {
    buffer += data;
}

// Метод возвращает текущий буфер клиента.
std::string Client::getBuffer() const {
    return buffer;
}

// Метод очищает буфер клиента до позиции pos (включая символ перевода строки).
// Используется после обработки полного сообщения, чтобы удалить обработанную часть.
void Client::clearBuffer(size_t pos) {
    buffer.erase(0, pos + 1);
}

// Метод установки текущего канала, в котором находится клиент.
// Это позволяет хранить информацию о том, на каком канале клиент в данный момент.
void Client::setCurrentChannel(const std::string &channel) {
    currentChannel = channel;
}

// Метод возвращает текущий канал, в котором находится клиент.
std::string Client::getCurrentChannel() const {
    return currentChannel;
}

// Метод проверяет, было ли уже отправлено welcome-сообщение клиенту.
bool Client::hasSentWelcome() const {
    return welcomeSent;
}

// Метод устанавливает флаг, что welcome-сообщение было отправлено.
// Это нужно, чтобы не отправлять приветствие повторно.
void Client::setSentWelcome(bool val) { 
    welcomeSent = val;
}