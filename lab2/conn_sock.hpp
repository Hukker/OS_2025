#pragma once
#include "common.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <string>

class ConnSock : public Conn {
private:
    int sock_fd;
    int client_fd;
    std::string socket_path;
    struct sockaddr_un addr;
    bool is_listening;
    
public:
    ConnSock(int id, bool create);
    ~ConnSock() override;
    
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;
    bool WaitForConnection() override;
};
