#include "common.hpp"
#include "chat_session.hpp"
#include "utils.hpp"
#include <memory>
#include <thread>

std::atomic<bool> client_running(true);

void client_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        Log("Получен сигнал handshake от хоста");
    } else if (sig == SIGINT || sig == SIGTERM) {
        Log("Завершение работы клиента");
        client_running = false;
    }
}

void send_handshake(pid_t host_pid) {
    Log("Отправка handshake сигнала хосту (PID: " + std::to_string(host_pid) + ")");
    kill(host_pid, SIGUSR1);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Использование: " << argv[0] 
                  << " <тип_соединения> <host_pid> <client_id> <имя_пользователя>" << std::endl;
        std::cerr << "Типы: mq, fifo, sock" << std::endl;
        return 1;
    }
    
    std::string conn_type = argv[1];
    pid_t host_pid = std::stoi(argv[2]);
    int client_id = std::stoi(argv[3]);
    std::string username = argv[4];
    
    struct sigaction sa;
    sa.sa_handler = client_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    send_handshake(host_pid);
    
    sem_t* global_semaphore = sem_open("/chat_semaphore", 0);
    if (global_semaphore == SEM_FAILED) {
        Log("Ошибка открытия семафора: " + std::string(strerror(errno)));
        return 1;
    }
    
    Log("Клиент запущен. Пользователь: " + username);
    
    std::unique_ptr<Conn> connection = createConnection(conn_type, client_id, false);
    if (!connection) {
        sem_close(global_semaphore);
        return 1;
    }
    
    Log("Подключение к хосту...");
    sleep(1);
    
    if (!connection->WaitForConnection()) {
        Log("Ошибка подключения к хосту");
        sem_close(global_semaphore);
        return 1;
    }
    
    Log("Подключение установлено");
    
    ChatSession session(std::move(connection), client_id, false, username);
    sleep(1); 
    
    while (session.isRunning() && client_running) {
        session.processIncomingMessage();
        
        std::cout << "> ";
        std::string input;
        std::getline(std::cin, input);
        
        if (!input.empty()) {
            session.processConsoleInput(input);
        }
        
        session.waitForMessage();
    }
    
    session.sendDisconnectMessage();
    sem_close(global_semaphore);
    
    Log("Клиент завершен");
    return 0;
}
