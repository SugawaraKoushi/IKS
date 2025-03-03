#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <WinSock2.h>
#include <string>
#include <thread>
#include <set>

#pragma comment(lib, "ws2_32.lib")

#pragma pack(1)

struct msg_t {
    short ver;      // версия
    short type;     // тип сообщения
    char text[1024]; // текст сообщения
};

#pragma pack()

/// <summary>
/// Получение сообщений
/// </summary>
void recieveMessages(SOCKET sock) {
    msg_t msg;
    sockaddr_in sin;
    int len = sizeof(sin);

    // Получим свой IP-адрес
    char hostname[256];
    int err = gethostname(hostname, sizeof(hostname));
    hostent* host = gethostbyname(hostname);

    std::set<std::string> localIPs;

    for (int i = 0; host->h_addr_list[i] != nullptr; i++) {
        localIPs.insert(inet_ntoa(*(in_addr*)host->h_addr_list[i]));
    }

    if (err == SOCKET_ERROR) {
        std::cout << "Ошибка при получении собстенного IP-адреса: " << WSAGetLastError() << std::endl;
    }

    while (true) {
        int n = recvfrom(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&sin, &len);

        if (n == SOCKET_ERROR) {
            std::cout << "Ошибка при получении сообщения: " << WSAGetLastError() << std::endl;
            err = WSAGetLastError();
            std::cout << err << std::endl;
        }
        else {
            std::string senderIP = inet_ntoa(sin.sin_addr);

            if (localIPs.find(senderIP) == localIPs.end()) {
                std::cout << "Сообщение успешно получено от " << senderIP << ": " << msg.text << std::endl;
            }
            else {
                std::cout << "Игнорируем собственное сообщение." << std::endl;
            }
        }
    }
}

int main()
{
    setlocale(LC_ALL, "");

    // Иницифализация WinSock
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (err != 0) {
        std::cout << "Ошибка при инициализации WinSock." << std::endl;
        return 1;
    }

    // Открытие сокета
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0); // IPv4, UDP-протокол

    if (sock == INVALID_SOCKET) {
        WSACleanup();
        std::cout << "Ошибка при открытии сокета." << std::endl;
        return 1;
    }

    // Ассоциирование сокета
    sockaddr_in localAddr = { 0 };
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(5150);
    localAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY); // Принимать сообщения на все интерфейсы

    err = bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr));

    if (err == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        std::cout << "Ошибка при привязке сокета." << std::endl;
        return 1;
    }

    // Установим широковещательный формат
    BOOL optval;
    int optlen = sizeof(optval);

    // Определяем значение флага SO_BROADCAST
    err = getsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, &optlen);

    // Если флаг false
    if ((err != SOCKET_ERROR) && (optval != TRUE)) {
        optval = TRUE;
        optlen = sizeof(optval);

        // Изменяем значение флага SO_BROADCAST
        err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, optlen);
    }

    if (err == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        std::cout << "Не удалось установить широковещательный формат" << std::endl;
        return 1;
    }

    // Запуск потока для приема сообщений
    std::thread recieveThread(recieveMessages, sock);

    // Подготовка структуры для отправки сообщений
    sockaddr_in broadcastAddr = { 0 };
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(5150);
    broadcastAddr.sin_addr.S_un.S_addr = htonl(INADDR_BROADCAST);

    msg_t msg; // буфер для отправки текстового сообщения
    msg.ver = 1;
    msg.type = 0;

    while (true) {
        std::cout << "Сообщение: ";
        std::string text;
        std::cin >> text;
        strcpy_s(msg.text, text.c_str());
        msg.text[sizeof(msg.text) - 1] = '\0';

        // Отправка текстового сообщения
        int n = sendto(sock, (const char *)&msg, sizeof(msg), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

        if (n != SOCKET_ERROR) {    // Сообщение успешно отправлено
            std::cout << "Сообщение успешно отправлено!" << std::endl;
        }
        else {
            std::cout << "Ошибка при отправке сообщения: " << WSAGetLastError() << std::endl;
        }

        msg.ver++;
    }

    recieveThread.join();
    
    closesocket(sock);
    WSACleanup();

    return 0;
}