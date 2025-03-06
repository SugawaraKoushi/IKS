#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>


#pragma comment(lib, "ws2_32.lib")

const int BUFFER_SIZE = 4096;

/// <summary>
/// Отправляет файл на сервер
/// </summary>
/// <param name="socket">сокет, по которому будут приходить данные от клиента на сервер</param>
/// <param name="path">путь к файлу</param>
void sendFile(SOCKET socket, std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cout << "Ошибка при открытии файла на чтение" << std::endl;
        return;
    }

    // Определим размер файла
    long fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    const char* ackMsg = "Отправляю данные";
    send(socket, ackMsg, strlen(ackMsg), 0);

    // Отправим имя файла на сервер
    std::string fileName = path.substr(path.find_last_of("\\") + 1);
    fileName += '\n';
    send(socket, fileName.c_str(), fileName.size(), 0);

    // Отправим размер файла на сервер
    std::string fileSizeStr = std::to_string(fileSize);
    fileSizeStr += '\n';
    send(socket, fileSizeStr.c_str(), fileSizeStr.size(), 0);

    // Отправим данные файла на сервер
    char buffer[BUFFER_SIZE];

    std::streamsize s = 0;

    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        send(socket, buffer, file.gcount(), 0);
        s += file.gcount();
        std::cout << "Отправлено байт:" << s << std::endl;
    }

    file.close();

    // Получим подтверждение об успешной отправке файла
    char msg[BUFFER_SIZE];
    int bytesRecived = recv(socket, msg, BUFFER_SIZE, 0);

    if (bytesRecived > 0) {
        msg[bytesRecived] = '\0';
        std::cout << "Сервер: " << msg << std::endl;
    }
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

    SOCKET clientSocket;

    // Открытие сокета
    clientSocket = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP-протокол

    if (clientSocket == INVALID_SOCKET) {
        WSACleanup();
        std::cout << "Ошибка при открытии сокета" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1234);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("26.47.32.61");

    // Подключаемся к серверу
    err = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // После подключения к серверу ожидаем от него ответ
    char ackBuffer[1024];
    int bytesReceived = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);

    if (bytesReceived > 0) {
        ackBuffer[bytesReceived] = '\0';
        std::cout << "Сервер: " << ackBuffer << std::endl;
    }

    if (err == SOCKET_ERROR) {
        closesocket(clientSocket);
        WSACleanup();
        std::cout << "Ошибка при подключении к серверу" << std::endl;
        return 1;
    }

    std::string path;
    std::cout << "Введите путь к файлу: ";
    std::cin >> path;

    sendFile(clientSocket, path);

    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
