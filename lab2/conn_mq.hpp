#pragma once
#include "common.hpp"
#include <mqueue.h>
#include <string>

class ConnMQ : public Conn {
private:
    mqd_t mq_descriptor;
    std::string queue_name;
    struct mq_attr attr;
    
public:
    ConnMQ(int id, bool create);
    ~ConnMQ() override;
    
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;
    bool WaitForConnection() override;
};
