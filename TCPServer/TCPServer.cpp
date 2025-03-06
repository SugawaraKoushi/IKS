#include <iostream>
#include <WinSock2.h>
#include <string>
#include <fstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

const int BUFFER_SIZE = 4096;

/// <summary>
/// Получение данных от клиента из буфера
/// </summary>
/// <param name="buffer">буффер с данными от клиента</param>
/// <param name="fileName">имя файла</param>
/// <param name="fileSize">размер файла</param>
/// <param name="remainingData">часть данных файла</param>
/// <returns>Получили ли все данные или нет</returns>
bool extractData(std::string& buffer, std::string& fileName, long& fileSize, std::string& remainingData) {
    size_t fileNameEnd = buffer.find('\n');

    if (fileNameEnd == std::string::npos) {
        return false; // Имя файла не полностью получено
    }

    fileName = buffer.substr(0, fileNameEnd);

    size_t fileSizeEnd = buffer.find('\n', fileNameEnd + 1);

    if (fileSizeEnd == std::string::npos) {
        return false; // Размер файла не полностью получен
    }

    std::string fileSizeStr = buffer.substr(fileNameEnd + 1, fileSizeEnd - (fileNameEnd + 1));
    fileSize = atol(fileSizeStr.c_str());

    // Сохраняем оставшиеся данные
    remainingData = buffer.substr(fileSizeEnd + 1);
    return true;
}

/// <summary>
/// Обрабатывает загрузку файлов от клиента
/// </summary>
/// <param name="clientSocket">Сокет клиента</param>
void handleClient(SOCKET clientSocket) {
    char tempBuffer[BUFFER_SIZE];
    std::string buffer; // Буфер для хранения данных
    std::string fileName;
    long fileSize = 0;
    std::string remainingData; // Оставшиеся данные после извлечения имени и размера файла

    while (true) {
        // Получаем данные от клиента
        int bytesReceived = recv(clientSocket, tempBuffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            std::cout << "Ошибка при получении данных" << std::endl;
            return;
        }

        // Добавляем данные в буфер
        buffer.append(tempBuffer, bytesReceived);

        // Пытаемся извлечь имя файла и размер файла
        if (extractData(buffer, fileName, fileSize, remainingData)) {
            std::cout << "Имя файла: " << fileName << std::endl;
            std::cout << "Размер файла: " << fileSize << std::endl;
            break; // Данные успешно извлечены
        }
    }

    // Открываем файл для записи
    std::ofstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        closesocket(clientSocket);
        std::cout << "Ошибка при открытии файла для записи" << std::endl;
        return;
    }

    // Записываем оставшиеся данные в файл
    if (!remainingData.empty()) {
        file.write(remainingData.c_str(), remainingData.size());
        fileSize -= remainingData.size();
        std::cout << "Записано байт из буфера: " << remainingData.size() << std::endl;
    }

    // Получаем оставшиеся данные файла
    long totalBytesReceived = remainingData.size();

    while (totalBytesReceived < fileSize) {
        int bytesReceived = recv(clientSocket, tempBuffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            file.close();
            closesocket(clientSocket);
            std::cout << "Ошибка при получении данных файла" << std::endl;
            return;
        }

        file.write(tempBuffer, bytesReceived);
        totalBytesReceived += bytesReceived;
        std::cout << "Получено байт: " << bytesReceived << " (всего: " << totalBytesReceived << ")" << std::endl;
    }

    file.close();

    std::cout << "Файл успешно получен" << std::endl;

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
