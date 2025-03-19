#include "ChatServer.hpp"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);  // Преобразуем строку в целое число для порта
    std::string password = argv[2];  // Пароль

    ChatServer server(port, password);  // Создаем объект ChatServer с портом и паролем
    server.run();

    return 0;
}