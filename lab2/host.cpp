#include "common.hpp"
#include "conn_mq.cpp"
#include "conn_fifo.cpp"
#include "conn_sock.cpp"
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

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

void handle_client(Conn* connection) {
    std::string client_name = "Client_1";
    Log("Клиент " + client_name + " подключился");
    
    sleep(1);
    
    ChatMessage welcome_msg;
    memset(&welcome_msg, 0, sizeof(welcome_msg));
    welcome_msg.type = MSG_CONNECT;
    welcome_msg.sender_id = 0;
    welcome_msg.recipient_id = 1;
    strcpy(welcome_msg.username, "Host");
    strcpy(welcome_msg.text, "Добро пожаловать в чат!");
    welcome_msg.timestamp = time(nullptr);
    
    if (!connection->Write(&welcome_msg, sizeof(welcome_msg))) {
        Log("Ошибка отправки приветственного сообщения");
    } else {
        Log("Приветственное сообщение отправлено");
    }
    
    while (running) {
        ChatMessage msg;
        memset(&msg, 0, sizeof(msg));
        
        if (connection->Read(&msg, sizeof(msg))) {
            msg.timestamp = time(nullptr);
            
            if (msg.type == MSG_PUBLIC) {
                Log("[" + std::string(msg.username) + "]: " + msg.text);
                
                ChatMessage echo_msg;
                memset(&echo_msg, 0, sizeof(echo_msg));
                echo_msg.type = MSG_PUBLIC;
                echo_msg.sender_id = 0;
                echo_msg.recipient_id = 1;
                strcpy(echo_msg.username, "Host");
                strcpy(echo_msg.text, ": ");
                strncat(echo_msg.text, msg.text, sizeof(echo_msg.text) - strlen(echo_msg.text) - 1);
                echo_msg.timestamp = time(nullptr);
                
                connection->Write(&echo_msg, sizeof(echo_msg));
                
            } else if (msg.type == MSG_PRIVATE) {
                Log("Личное от " + std::string(msg.username) + 
                    " для " + std::to_string(msg.recipient_id) + 
                    ": " + msg.text);
            } else if (msg.type == MSG_DISCONNECT) {
                Log("Клиент " + client_name + " запросил отключение");
                break;
            } else if (msg.type == MSG_CONNECT) {
                Log("Клиент " + std::string(msg.username) + " присоединился");
            }
        }
        
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        
        struct timeval timeout = {0, 100000}; 
        
        if (select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout) > 0) {
            std::string input;
            std::getline(std::cin, input);
            
            if (!input.empty()) {
                if (input == "/exit") {
                    running = false;
                    break;
                }
                
                ChatMessage host_msg;
                memset(&host_msg, 0, sizeof(host_msg));
                host_msg.type = MSG_PUBLIC;
                host_msg.sender_id = 0;
                host_msg.recipient_id = 1;
                strcpy(host_msg.username, "Host");
                strncpy(host_msg.text, input.c_str(), sizeof(host_msg.text) - 1);
                host_msg.text[sizeof(host_msg.text) - 1] = '\0';
                host_msg.timestamp = time(nullptr);
                
                if (connection->Write(&host_msg, sizeof(host_msg))) {
                    Log("[Host]: " + input);
                } else {
                    Log("Ошибка отправки сообщения");
                }
            }
        }
        
        usleep(100000);
    }
    
    Log("Клиент " + client_name + " отключился");
    delete connection;
    
    running = false;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <тип_соединения> <имя_пользователя>" << std::endl;
        std::cerr << "Типы: mq, fifo, sock" << std::endl;
        return 1;
    }
    
    std::string conn_type = argv[1];
    std::string username = argv[2];
    
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGUSR1, &sa, NULL) == -1) {
        std::cerr << "Ошибка установки обработчика сигналов" << std::endl;
        return 1;
    }
    
    global_semaphore = sem_open("/chat_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (global_semaphore == SEM_FAILED && errno == EEXIST) {
        sem_unlink("/chat_semaphore");
        global_semaphore = sem_open("/chat_semaphore", O_CREAT, 0666, 1);
    }
    
    if (global_semaphore == SEM_FAILED) {
        Log("Ошибка создания семафора: " + std::string(strerror(errno)));
        return 1;
    }
    
    Log("Хост-сервер запущен. Пользователь: " + username);
    Log("Тип соединения: " + conn_type);
    Log("PID хоста: " + std::to_string(getpid()));
    Log("Ожидание подключения клиента...");
    Log("Запустите клиента в другом терминале:");
    Log("./client_" + conn_type + " " + conn_type + " " + 
        std::to_string(getpid()) + " <имя_пользователя>");
    
    Conn* connection = nullptr;
    
    if (conn_type == "mq") {
        connection = new ConnMQ(1, true);
    } else if (conn_type == "fifo") {
        connection = new ConnFIFO(1, true, true); 
    } else if (conn_type == "sock") {
        connection = new ConnSock(1, true);
    } else {
        Log("Неизвестный тип соединения: " + conn_type);
        sem_close(global_semaphore);
        sem_unlink("/chat_semaphore");
        return 1;
    }
    
    if (connection == nullptr) {
        Log("Ошибка создания соединения");
        sem_close(global_semaphore);
        sem_unlink("/chat_semaphore");
        return 1;
    }
    
    Log("Ожидание подключения клиента...");
    bool connected = false;
    
    while (!connected && running) {
        if (connection->WaitForConnection()) {
            connected = true;
            Log("Клиент подключился!");
            break;
        }
        
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        
        struct timeval timeout = {1, 0}; 
        
        if (select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout) > 0) {
            std::string input;
            std::getline(std::cin, input);
            if (input == "/exit") {
                running = false;
                break;
            }
        }
        
        if (running) {
            Log("Ожидание подключения... (введите /exit для выхода)");
        }
    }
    
    if (!connected) {
        Log("Клиент не подключился");
        delete connection;
        sem_close(global_semaphore);
        sem_unlink("/chat_semaphore");
        return 1;
    }
    
    Log("Для отправки сообщений введите текст и нажмите Enter");
    Log("Для выхода введите /exit");
    
    handle_client(connection);
    
    if (global_semaphore != nullptr) {
        sem_close(global_semaphore);
        sem_unlink("/chat_semaphore");
    }
    
    Log("Хост-сервер завершен");
    return 0;
}