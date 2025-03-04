#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <WinSock2.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

const int message_len = 10;

#pragma pack(1)

struct msg_t {
    short ver;          // версия (позиция)
    short type;         // тип сообщения
    short len;          // длина сообщений
    char text[message_len];     // текст сообщения
};

#pragma pack()

// Получение списка IP-адресов для указанного узла
void getHostAddr(const char* hostname, std::vector<std::string> &ips) {
    hostent* host = gethostbyname(hostname);

    for (int i = 0; host->h_addr_list[i] != nullptr; i++) {
        ips.push_back(inet_ntoa(*(in_addr*)host->h_addr_list[i]));
    }
}

/// <summary>
/// Делит текст сообщения на части
/// </summary>
/// <param name="text">Текст</param>
/// <returns>Список сообщений для отправки</returns>
std::vector<msg_t> splitTextIntoMessages(std::string text) {
    std::vector<msg_t> messages;
    short ver = 1;
    short parts = 0;

    if (text.length() % message_len >= 0) {
        parts++;
    }

    parts += text.length() / message_len;
    
    for (size_t i = 0; i < text.length(); i += message_len) {
        msg_t msg;
        msg.type = 1;
        msg.ver = ver++;
        msg.len = parts;
        strcpy_s(msg.text, text.c_str());
        msg.text[sizeof(msg.text) - 1] = '\0';
        messages.push_back(msg);
    }

    return messages;
}

/// <summary>
/// Получает сообщения
/// </summary>
/// <param name="sock">Сокет</param>
void recieveMessages(SOCKET sock) {
    msg_t msg;
    sockaddr_in sin;
    int len = sizeof(sin);

    // Получим свой IP-адрес
    char hostname[256];
    int err = gethostname(hostname, sizeof(hostname));
    std::vector<std::string> ips;
    getHostAddr(hostname, ips);

    while (true) {
        std::string text;
        std::string senderIP;

        // Формируем одно сообщение из частей
        do {
            int n = recvfrom(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&sin, &len);
            

            if (n == SOCKET_ERROR) {
                std::cout << "Ошибка при получении сообщения: " << WSAGetLastError() << std::endl;
            }
            else {
                senderIP = inet_ntoa(sin.sin_addr);

                if (std::find(ips.begin(), ips.end(), senderIP) == ips.end()) {
                    text += msg.text;
                }
            }
        } while (msg.ver != msg.len);
        
        if (std::find(ips.begin(), ips.end(), senderIP) == ips.end()) {
            std::cout << "[" << senderIP << "]: " << text << std::endl;
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

    std::string broadcastMode;
    std::cout << "Установить широковещательный формат? (y / n)" << std::endl;

    do {
        std::cin >> broadcastMode;
    } while (broadcastMode[0] != 'y' && broadcastMode[0] != 'n');

    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5150);
    std::string reciever;

    if (broadcastMode[0] == 'y') { // Установим широковещательный формат
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

        // Подготовка структуры для отправки сообщений
        addr.sin_addr.S_un.S_addr = htonl(INADDR_BROADCAST);
    }
    else {  // Введем адрес или имя узла вручную
        std::cout << "Введите IP-адрес получателя или его имя: ";
        std::cin >> reciever;
        bool isDone = false;

        do {
            // Пользователь ввел IP-адрес?
            err = inet_addr(reciever.c_str());
            
            if (err == INADDR_NONE) {
                std::cout << "IP-адреса" << reciever << "не существует" << std::endl;
            }
            else {
                isDone = true;
            }

            // Пользователь ввел имя узла?
            std::vector<std::string> ips;
            getHostAddr(reciever.c_str(), ips);

            if (!ips.empty()) { // Нашли нужный IP-адрес по имени узла
                reciever = ips.at(0);
                isDone = true;
            }
        } while (!isDone);

        addr.sin_addr.S_un.S_addr = inet_addr(reciever.c_str());
    }

    // Запуск потока для приема сообщений
    std::thread recieveThread(recieveMessages, sock);

    msg_t msg; // буфер для отправки текстового сообщения
    msg.ver = 1;
    msg.type = 0;

    while (true) {
        std::cout << "Сообщение: ";
        std::string text;
        std::cin >> text;
        
        // Разделим сообщения на части
        std::vector<msg_t> messages = splitTextIntoMessages(text);

        for (size_t i = 0; i < messages.size(); i++) {  // Отправка сообщений
            int n = sendto(sock, (const char*)&msg, sizeof(msg), 0, (struct sockaddr*)&addr, sizeof(addr));

            if (n == SOCKET_ERROR) {    // Сообщение не получилось отправить
                std::cout << "Ошибка при отправке сообщения: " << WSAGetLastError() << std::endl;
            }
        }

        msg.ver++;
    }

    recieveThread.join();
    
    closesocket(sock);
    WSACleanup();

    return 0;
}