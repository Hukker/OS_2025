#include "common.hpp"

class ConnSock : public Conn {
private:
    int sock_fd;
    int client_fd;
    std::string socket_path;
    struct sockaddr_un addr;
    bool is_listening;
    
public:
    ConnSock(int id, bool create) : Conn(id, create), client_fd(-1), is_listening(false) {
        socket_path = "/tmp/chat_sock_" + std::to_string(id);
        
        sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock_fd == -1) {
            Log("Ошибка создания сокета: " + std::string(strerror(errno)));
            return;
        }
        
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        
        if (create) {
            unlink(socket_path.c_str());
            if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
                Log("Ошибка bind: " + std::string(strerror(errno)));
                close(sock_fd);
                return;
            }
            
            if (listen(sock_fd, 1) == -1) { 
                Log("Ошибка listen: " + std::string(strerror(errno)));
                close(sock_fd);
                return;
            }
            is_listening = true;
        }
    }
    
    bool WaitForConnection() override {
        if (is_creator && is_listening) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(sock_fd, &read_set);
            
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            
            int result = select(sock_fd + 1, &read_set, NULL, NULL, &timeout);
            if (result <= 0) return false;
            
            client_fd = accept(sock_fd, NULL, NULL);
            return client_fd != -1;
        } else if (!is_creator) {
            if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
                return false;
            }
            client_fd = sock_fd;
            return true;
        }
        return false;
    }
    
    bool Read(void *buf, size_t count) override {
        if (client_fd == -1) return false;
        
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(client_fd, &read_set);
        
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        int result = select(client_fd + 1, &read_set, NULL, NULL, &timeout);
        if (result <= 0) return false;
        
        return recv(client_fd, buf, count, 0) == (ssize_t)count;
    }
    
    bool Write(const void *buf, size_t count) override {
        if (client_fd == -1) return false;
        
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(client_fd, &write_set);
        
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        int result = select(client_fd + 1, NULL, &write_set, NULL, &timeout);
        if (result <= 0) return false;
        
        return send(client_fd, buf, count, 0) == (ssize_t)count;
    }
    
    ~ConnSock() {
        if (client_fd != -1 && client_fd != sock_fd) {
            close(client_fd);
        }
        if (sock_fd != -1) {
            close(sock_fd);
        }
        if (is_creator) {
            unlink(socket_path.c_str());
        }
    }
};