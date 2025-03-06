#include <iostream>
#include <WinSock2.h>
#include <string>
#include <fstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

const int BUFFER_SIZE = 4096;

/// <summary>
/// Обрабатывает загрузку файлов от клиента
/// </summary>
/// <param name="clientSocket">Сокет клиента</param>
void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    // Получим имя файла
    bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        std::cout << "Ошибка при получении имени файла" << std::endl;
        return;
    }

    std::string fileName(buffer, bytesReceived);

    // Получим размер файла
    bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        std::cout << "Ошибка при получении размера файла" << std::endl;
        return;
    }

    long fileSize = atol(buffer);

    // Откроем файл для записи
    std::ofstream file(fileName, std::ios::binary);

    if (!file.is_open()) {
        closesocket(clientSocket);
        std::cout << "Ошибка при открытии файла для записи" << std::endl;
        return;
    }

    // Получим данные файла
    long totalBytesReceived = 0;
    
    while (totalBytesReceived < fileSize) {
        bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            file.close();
            closesocket(clientSocket);
            std::cout << "Ошибка при получении данных файла" << std::endl;
            return;
        }

        file.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;

        // Вывод прогресса
        double progress = totalBytesReceived / fileSize * 100;
        std::cout << "Прогресс: " << progress << "%" << std::endl;
    }

    file.close();

    if (totalBytesReceived != fileSize) {
        std::cerr << "Файл передан не полностью. Ожидалось: " << fileSize << ", получено: " << totalBytesReceived << std::endl;
        const char* msg = "Файл получен неполностью";
        send(clientSocket, msg, strlen(msg), 0);
    }
    else {
        std::cout << "Файл " << fileName << " успешно получен" << std::endl;
        const char* msg = "Файл получен";
        send(clientSocket, msg, strlen(msg), 0);
    }

    closesocket(clientSocket);
}

int main() {
    setlocale(LC_ALL, "");

    WSADATA wsaData;

    // Иницифализация WinSock
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (err != 0) {
        std::cout << "Ошибка при инициализации WinSock." << std::endl;
        return 1;
    }

    SOCKET serverSocket;

    // Открытие сокета
    serverSocket = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP-протокол

    if (serverSocket == INVALID_SOCKET) {
        WSACleanup();
        std::cout << "Ошибка при открытии сокета" << std::endl;
        return 1;
    }

    // Ассоциирование сокета
    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY); // Принимать сообщения на все интерфейсы

    err = bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr));

    if (err == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        std::cout << "Ошибка при привязке сокета" << std::endl;
        return 1;
    }

    // Перевод сокета в режим прослушивания
    err = listen(serverSocket, SOMAXCONN);

    if (err == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        std::cout << "Ошибка при переводе сокета в режим прослушивания" << std::endl;
        return 1;
    }

    std::cout << "Сервер запущен и ожидает подключений..." << std::endl;

    while (true) {
        // Открываем соединения
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Ошибка при открытии соединения" << std::endl;
        }

        // На каждое соединение создаем отдельный поток
        std::thread(handleClient, clientSocket).detach();
    }

    shutdown(serverSocket, SD_BOTH);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
