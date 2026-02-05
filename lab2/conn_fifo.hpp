#pragma once

#include "common.hpp"
#include <string>

class ConnFIFO : public Conn {
private:
    int fd;
    std::string fifo_name;
    bool is_reading;
    
public:
    ConnFIFO(int id, bool create, bool for_reading = false);
    ~ConnFIFO() override;
    
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;
    bool WaitForConnection() override;
};
