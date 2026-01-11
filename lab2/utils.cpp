#include "utils.hpp"
#include "common.hpp"
#include "conn_fifo.hpp"
#include "conn_mq.hpp"
#include "conn_sock.hpp"
#include "chat_session.hpp"
#include <iostream>

void host_signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        Log("Получен сигнал завершения");
    } else if (sig == SIGUSR1) {
        Log("Получен handshake от клиента");
    }
}

void client_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        Log("Получен сигнал handshake от хоста");
    } else if (sig == SIGINT || sig == SIGTERM) {
        Log("Завершение работы клиента");
    }
}

void setupHostSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = host_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
}

void setupClientSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = client_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

sem_t* createGlobalSemaphore() {
    sem_t* sem = sem_open("/chat_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (sem == SEM_FAILED && errno == EEXIST) {
        sem_unlink("/chat_semaphore");
        sem = sem_open("/chat_semaphore", O_CREAT, 0666, 1);
    }
    
    if (sem == SEM_FAILED) {
        Log("Ошибка создания семафора: " + std::string(strerror(errno)));
    }
    
    return sem;
}

void cleanupResources(sem_t* semaphore) {
    if (semaphore != nullptr) {
        sem_close(semaphore);
        sem_unlink("/chat_semaphore");
    }
}

std::unique_ptr<Conn> createConnection(const std::string& type, int id, bool create) {
    if (type == "mq") {
        return std::make_unique<ConnMQ>(id, create);
    } else if (type == "fifo") {
        return std::make_unique<ConnFIFO>(id, create, create); 
    } else if (type == "sock") {
        return std::make_unique<ConnSock>(id, create);
    }
    
    Log("Неизвестный тип соединения: " + type);
    return nullptr;
}

bool waitForClientConnection(Conn* connection) {
    Log("Ожидание подключения клиента...");
    
    for (int attempt = 0; attempt < 30; attempt++) {
        Log("Попытка подключения " + std::to_string(attempt + 1) + "...");
        
        if (connection->WaitForConnection()) {
            return true;
        }
        
        std::string input = getConsoleInput(1000);
        if (input == "/exit") {
            return false;
        }
        
        sleep(1);
    }
    
    Log("Таймаут ожидания подключения клиента");
    return false;
}

std::string getConsoleInput(int timeout_ms) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout) > 0) {
        std::string input;
        std::getline(std::cin, input);
        return input;
    }
    
    return "";
}