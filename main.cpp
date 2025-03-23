#include "ChatServer.hpp"
#include <climits>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

	errno = 0;
	char *end;
    // int port = std::stoi(argv[1]);  // Преобразуем строку в целое число для порта
	long result = std::strtol(argv[1], &end, 10); // Преобразуем строку в целое число для порта

	if (errno == ERANGE || result > INT_MAX || result < 1024)
	{
        std::cerr << "Invalid port value!" << std::endl;
        return 1;
	}

	if (*end != '\0')
	{
        std::cerr << "Invalid port value!" << std::endl;
        return 1;
	}

	int port = static_cast<int>(result);

    std::string password = argv[2];  // Пароль

    ChatServer server(port, password);  // Создаем объект ChatServer с портом и паролем
    server.run();

    return 0;
}