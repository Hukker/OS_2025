#include "common.hpp"
#include <mqueue.h>

class ConnMQ : public Conn {
private:
    mqd_t mq_descriptor;
    std::string queue_name;
    struct mq_attr attr;
    
public:
    ConnMQ(int id, bool create) : Conn(id, create) {
        queue_name = "/chat_mq_" + std::to_string(id);
        
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = sizeof(ChatMessage);
        attr.mq_curmsgs = 0;
        
        if (create) {
            mq_unlink(queue_name.c_str());
            mq_descriptor = mq_open(queue_name.c_str(), 
                                   O_CREAT | O_RDWR | O_NONBLOCK, 
                                   0666, &attr);
        } else {
            mq_descriptor = mq_open(queue_name.c_str(), O_RDWR);
        }
        
        if (mq_descriptor == (mqd_t)-1) {
            Log("Ошибка открытия очереди: " + std::string(strerror(errno)));
        }
    }
    
    bool Read(void *buf, size_t count) override {
        if (count != sizeof(ChatMessage)) return false;
        
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 5;
        
        int result = mq_timedreceive(mq_descriptor, 
                                    (char*)buf, 
                                    sizeof(ChatMessage), 
                                    NULL, 
                                    &timeout);
        return result != -1;
    }
    
    bool Write(const void *buf, size_t count) override {
        if (count != sizeof(ChatMessage)) return false;
        
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 5;
        
        int result = mq_timedsend(mq_descriptor, 
                                 (const char*)buf, 
                                 sizeof(ChatMessage), 
                                 0, 
                                 &timeout);
        return result != -1;
    }
    
    bool WaitForConnection() override {
        if (is_creator) {
            ChatMessage test_msg;
            memset(&test_msg, 0, sizeof(test_msg));
            
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1; 
            
            return mq_timedsend(mq_descriptor, 
                               (const char*)&test_msg, 
                               sizeof(ChatMessage), 
                               0, 
                               &timeout) != -1;
        } else {
            return mq_descriptor != (mqd_t)-1;
        }
    }
    
    ~ConnMQ() {
        if (mq_descriptor != (mqd_t)-1) {
            mq_close(mq_descriptor);
        }
        if (is_creator) {
            mq_unlink(queue_name.c_str());
        }
    }
};