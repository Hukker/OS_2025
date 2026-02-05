#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <mqueue.h>
#include <poll.h>
#include <thread>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <errno.h>


#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define TIMEOUT_MS 5000
#define INACTIVITY_TIMEOUT 60


enum MessageType {
    MSG_CONNECT = 1,
    MSG_DISCONNECT = 2,
    MSG_PUBLIC = 3,
    MSG_PRIVATE = 4,
    MSG_HANDSHAKE = 5,
    MSG_USER_LIST = 6
};

struct ChatMessage {
    MessageType type;
    int sender_id;
    int recipient_id; 
    char username[32];
    char text[256];
    time_t timestamp;
};

class Conn {
public:
    Conn(int id, bool create) : conn_id(id), is_creator(create) {}
    virtual ~Conn() {}
    
    virtual bool Read(void *buf, size_t count) = 0;
    virtual bool Write(const void *buf, size_t count) = 0;
    virtual bool WaitForConnection() = 0;
    
protected:
    int conn_id;
    bool is_creator;
};

static inline void Log(const std::string& message) {
    time_t now = time(nullptr);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    std::cout << "[" << time_str << "] " << message << std::endl;
}

