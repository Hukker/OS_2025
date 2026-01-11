#include "common.hpp"
#include "conn_fifo.hpp"
#include "conn_mq.hpp"
#include "conn_sock.hpp"
#include "chat_session.hpp"
#include "utils.hpp"
#include <memory>
#include <csignal>
#include <atomic>
#include <semaphore.h>

std::atomic<bool> running(true);
sem_t *global_semaphore = nullptr;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        Log("Получен сигнал завершения");
        running = false;
    } else if (sig == SIGUSR1) {
        Log("Получен handshake от клиента");
    }
}

void setupSignalHandlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] 
                  << " <тип_соединения> <имя_пользователя>" << std::endl;
        std::cerr << "Типы: mq, fifo, sock" << std::endl;
        return 1;
    }
    
    std::string conn_type = argv[1];
    std::string username = argv[2];
    
    setupSignalHandlers();
    
    global_semaphore = createGlobalSemaphore();
    
    Log("Хост-сервер запущен. Пользователь: " + username);
    Log("Тип соединения: " + conn_type);
    Log("PID хоста: " + std::to_string(getpid()));
    
    std::unique_ptr<Conn> connection = createConnection(conn_type, 1, true);
    if (!connection) {
        cleanupResources(global_semaphore);
        return 1;
    }
    
    if (!waitForClientConnection(connection.get())) {
        cleanupResources(global_semaphore);
        return 1;
    }
    
    Log("Клиент подключился успешно!");
    
    ChatSession session(std::move(connection), 1, true, username);
    session.sendWelcomeMessage();
    
    Log("Для отправки сообщений введите текст и нажмите Enter");
    Log("Для выхода введите /exit");
    
    while (session.isRunning() && running) {
        if (!session.processIncomingMessage()) {
            break;
        }
        
        std::string input = getConsoleInput();
        if (!input.empty()) {
            session.processConsoleInput(input);
        }
        
        session.waitForMessage();
    }
    
    session.sendDisconnectMessage();
    cleanupResources(global_semaphore);
    
    Log("Хост-сервер завершен");
    return 0;
}