#include "common.hpp"
#include <thread>

#include "conn_mq.cpp"
#include "conn_fifo.cpp"
#include "conn_sock.cpp"

std::atomic<bool> client_running(true);
pid_t host_pid;
std::string username;
sem_t *global_semaphore = nullptr;

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        Log("Получен сигнал handshake от хоста");
    } else if (sig == SIGINT || sig == SIGTERM) {
        Log("Завершение работы клиента");
        client_running = false;
    }
}

void send_handshake() {
    Log("Отправка handshake сигнала хосту (PID: " + std::to_string(host_pid) + ")");
    kill(host_pid, SIGUSR1);
}

void message_receiver(Conn* connection) {
    while (client_running) {
        ChatMessage msg;
        memset(&msg, 0, sizeof(msg));
        
        if (sem_wait(global_semaphore) == -1) {
            continue;
        }
        
        if (connection->Read(&msg, sizeof(msg))) {
            switch (msg.type) {
                case MSG_CONNECT:
                    std::cout << "\n=== " << msg.text << " ===\n" << std::endl;
                    std::cout << "> " << std::flush;
                    break;
                case MSG_DISCONNECT:
                    std::cout << "\nСервер отключился" << std::endl;
                    client_running = false;
                    break;
                case MSG_PUBLIC:
                    std::cout << "\n[" << msg.username << "]: " << msg.text << std::endl;
                    std::cout << "> " << std::flush;
                    break;
                case MSG_PRIVATE:
                    std::cout << "\n[Личное от " << msg.username << "]: " 
                              << msg.text << std::endl;
                    std::cout << "> " << std::flush;
                    break;
                default:
                    break;
            }
        }
        
        sem_post(global_semaphore);
        usleep(100000);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] 
                  << " <тип_соединения> <host_pid> <имя_пользователя>" << std::endl;
        std::cerr << "Типы: mq, fifo, sock" << std::endl;
        std::cerr << "Пример: ./client_fifo fifo 12345 User1" << std::endl;
        return 1;
    }
    
    std::string conn_type = argv[1];
    host_pid = atoi(argv[2]);
    username = argv[3];
    int client_id = 1; 
    
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &sa, NULL) == -1 ||
        sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        std::cerr << "Ошибка установки обработчика сигналов" << std::endl;
        return 1;
    }
    
    send_handshake();
    
    global_semaphore = sem_open("/chat_semaphore", 0);
    if (global_semaphore == SEM_FAILED) {
        Log("Ошибка открытия семафора: " + std::string(strerror(errno)));
        return 1;
    }
    
    Log("Клиент запущен. Пользователь: " + username);
    
    Conn* connection = nullptr;
    
    if (conn_type == "mq") {
        connection = new ConnMQ(client_id, false);
    } else if (conn_type == "fifo") {
        connection = new ConnFIFO(client_id, false, false); 
    } else if (conn_type == "sock") {
        connection = new ConnSock(client_id, false);
    } else {
        Log("Неизвестный тип соединения");
        sem_close(global_semaphore);
        return 1;
    }
    
    if (connection == nullptr) {
        Log("Ошибка создания соединения");
        sem_close(global_semaphore);
        return 1;
    }
    
    Log("Подключение к хосту...");
    
    bool connected = false;
    for (int attempt = 0; attempt < 30 && !connected; attempt++) {
        Log("Попытка подключения " + std::to_string(attempt + 1) + "...");
        if (connection->WaitForConnection()) {
            connected = true;
            break;
        }
        sleep(1);
    }
    
    if (!connected) {
        Log("Таймаут ожидания подключения к хосту");
        delete connection;
        sem_close(global_semaphore);
        return 1;
    }
    
    Log("Подключение установлено");
    
    sleep(1);
    
    ChatMessage connect_msg;
    memset(&connect_msg, 0, sizeof(connect_msg));
    connect_msg.type = MSG_CONNECT;
    connect_msg.sender_id = client_id;
    connect_msg.recipient_id = 0;
    strncpy(connect_msg.username, username.c_str(), sizeof(connect_msg.username) - 1);
    strncpy(connect_msg.text, "присоединился к чату", sizeof(connect_msg.text) - 1);
    connect_msg.timestamp = time(nullptr);
    
    if (!connection->Write(&connect_msg, sizeof(connect_msg))) {
        Log("Ошибка отправки сообщения о подключении");
    } else {
        Log("Сообщение о подключении отправлено");
    }
    
    std::thread receiver(message_receiver, connection);
    
    while (client_running) {
        std::cout << "> ";
        std::string input;
        std::getline(std::cin, input);
        
        if (!client_running) break;
        
        if (input.empty()) continue;
        
        if (input == "/exit") {
            client_running = false;
            break;
        } else if (input == "/help") {
            std::cout << "Команды:\n";
            std::cout << "  /exit - выход\n";
            std::cout << "  /help - справка\n";
            std::cout << "  <текст> - публичное сообщение\n";
            continue;
        }
        
        ChatMessage msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.type = MSG_PUBLIC;
        msg.sender_id = client_id;
        msg.recipient_id = 0;
        strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
        strncpy(msg.text, input.c_str(), sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.timestamp = time(nullptr);
        
        if (sem_wait(global_semaphore) == -1) {
            continue;
        }
        
        bool success = connection->Write(&msg, sizeof(msg));
        
        sem_post(global_semaphore);
        
        if (!success) {
            Log("Ошибка отправки сообщения");
        }
    }
    
    ChatMessage disconnect_msg;
    memset(&disconnect_msg, 0, sizeof(disconnect_msg));
    disconnect_msg.type = MSG_DISCONNECT;
    disconnect_msg.sender_id = client_id;
    strncpy(disconnect_msg.username, username.c_str(), sizeof(disconnect_msg.username) - 1);
    strncpy(disconnect_msg.text, "покинул чат", sizeof(disconnect_msg.text) - 1);
    disconnect_msg.timestamp = time(nullptr);
    
    connection->Write(&disconnect_msg, sizeof(disconnect_msg));
    
    client_running = false;
    if (receiver.joinable()) {
        receiver.join();
    }
    
    delete connection;
    sem_close(global_semaphore);
    
    Log("Клиент завершен");
    return 0;
}