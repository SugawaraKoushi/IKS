#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <WinSock2.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

const int message_len = 10; // Ограничитель текста сообщения

#pragma pack(1)

struct msg_t {
    short ver;          // версия (какая часть сообщения)
    short len;          // длина сообщений
    char text[message_len + 1];     // текст сообщения
};

#pragma pack()

/// <summary>
/// Начальная инициализация приложения
/// </summary>
/// <param name="wsaData"></param>
/// <param name="sock"></param>
void socketSetup(WSADATA &wsaData, SOCKET &sock) {

    // Иницифализация WinSock
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (err != 0) {
        std::cout << "Ошибка при инициализации WinSock." << std::endl;
        exit(1);
    }

    // Открытие сокета
    sock = socket(AF_INET, SOCK_DGRAM, 0); // IPv4, UDP-протокол

    if (sock == INVALID_SOCKET) {
        WSACleanup();
        std::cout << "Ошибка при открытии сокета." << std::endl;
        exit(1);
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
        exit(1);
    }
}

/// <summary>
/// Получение списка IP-адресов для указанного узла
/// </summary>
/// <param name="hostname">Имя хоста</param>
/// <param name="ips">список IP-адресов для hostname</param>
void getHostAddr(const char* hostname, std::vector<std::string> &ips) {
    hostent* host = gethostbyname(hostname);

    for (int i = 0; host->h_addr_list[i] != nullptr; i++) {
        ips.push_back(inet_ntoa(*(in_addr*)host->h_addr_list[i]));
    }
}

/// <summary>
/// Устанавливает широковещательный формат для сокета
/// </summary>
/// <param name="sock">Сокет</param>
void setBroadcastMode(SOCKET sock) {
    BOOL optval;
    int optlen = sizeof(optval);

    // Определяем значение флага SO_BROADCAST
    int err = getsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, &optlen);

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
        exit(1);
    }
}

bool findRecieverAddress(std::string &reciever) {
    // Пользователь ввел IP-адрес?
    int err = inet_addr(reciever.c_str());

    if (err != INADDR_NONE) {
        return true;
    }

    // Пользователь ввел имя узла?
    std::vector<std::string> ips;
    getHostAddr(reciever.c_str(), ips);

    if (!ips.empty()) { // Нашли нужный IP-адрес по имени узла
        reciever = ips.at(0);
        return true;
    }

    return false;
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

    if (text.length() % message_len > 0) {
        parts++;
    }

    parts += text.length() / message_len;
    
    for (size_t i = 0; i < text.length(); i += message_len) {
        msg_t msg;
        msg.ver = ver++;
        msg.len = parts;
        std::string substr = text.substr(i, message_len).c_str();
        strcpy_s(msg.text, substr.c_str());
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

/// <summary>
/// Отправляет сообщения
/// </summary>
/// <param name="sock">Сокет</param>
/// <param name="addr">Адрес отправки</param>
void sendMessages(SOCKET sock, sockaddr_in addr) {
    while (true) {
        std::cout << "Сообщение: ";
        std::string text;
        std::getline(std::cin, text);

        // Разделим сообщения на части
        std::vector<msg_t> messages = splitTextIntoMessages(text);

        for (size_t i = 0; i < messages.size(); i++) {  // Отправка сообщений
            int n = sendto(sock, (const char*)&messages.at(i), sizeof(messages.at(i)), 0, (struct sockaddr*)&addr, sizeof(addr));

            if (n == SOCKET_ERROR) {    // Сообщение не получилось отправить
                std::cout << "Ошибка при отправке сообщения: " << WSAGetLastError() << std::endl;
            }
        }
    }
}

int main()
{
    setlocale(LC_ALL, "");

    WSADATA wsaData;
    SOCKET sock;

    socketSetup(wsaData, sock);

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
        setBroadcastMode(sock);

        // Подготовка структуры для отправки сообщений
        addr.sin_addr.S_un.S_addr = htonl(INADDR_BROADCAST);
    }
    else {  // Введем адрес или имя узла вручную
        std::cout << "Введите IP-адрес получателя или его имя: ";
        std::cin >> reciever;
        bool isDone = false;

        do {
            isDone = findRecieverAddress(reciever);
        } while (!isDone);

        addr.sin_addr.S_un.S_addr = inet_addr(reciever.c_str());
    }

    // Запуск потока для приема сообщений
    std::thread recieveThread(recieveMessages, sock);

    // Отправка сообщений
    sendMessages(sock, addr);

    recieveThread.join();
    closesocket(sock);
    WSACleanup();
    return 0;
}