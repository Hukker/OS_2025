#include "conn_fifo.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>
#include <string>

ConnFIFO::ConnFIFO(int id, bool create, bool for_reading) 
    : Conn(id, create), is_reading(for_reading) {
    
    fifo_name = "/tmp/chat_fifo_" + std::to_string(id);
    
    if (create) {
        unlink(fifo_name.c_str());
        if (mkfifo(fifo_name.c_str(), 0666) == -1) {
            Log("Ошибка создания FIFO: " + std::string(strerror(errno)));
        }
        
        if (is_reading) {
            fd = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
        } else {
            fd = open(fifo_name.c_str(), O_WRONLY | O_NONBLOCK);
        }
    } else {
        if (is_reading) {
            fd = open(fifo_name.c_str(), O_WRONLY | O_NONBLOCK);
        } else {
            fd = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
        }
    }
    
    if (fd == -1) {
        Log("Ошибка открытия FIFO: " + std::string(strerror(errno)));
    }
}

ConnFIFO::~ConnFIFO() {
    if (fd != -1) {
        close(fd);
    }
    if (is_creator) {
        unlink(fifo_name.c_str());
    }
}

bool ConnFIFO::Read(void *buf, size_t count) {
    if (!is_reading) return false;
    
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int result = select(fd + 1, &read_set, NULL, NULL, &timeout);
    if (result <= 0) return false;
    
    ssize_t bytes_read = read(fd, buf, count);
    return bytes_read == (ssize_t)count;
}

bool ConnFIFO::Write(const void *buf, size_t count) {
    if (is_reading) return false;
    
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(fd, &write_set);
    
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int result = select(fd + 1, NULL, &write_set, NULL, &timeout);
    if (result <= 0) return false;
    
    ssize_t bytes_written = write(fd, buf, count);
    return bytes_written == (ssize_t)count;
}

bool ConnFIFO::WaitForConnection() {
    if (is_creator) {
        struct stat st;
        if (stat(fifo_name.c_str(), &st) == -1) {
            return false; 
        }
        
        int test_fd = open(fifo_name.c_str(), O_WRONLY | O_NONBLOCK);
        if (test_fd != -1) {
            close(test_fd);
            return true;
        }
        return false;
    } else {
        struct stat st;
        return stat(fifo_name.c_str(), &st) == 0;
    }
}