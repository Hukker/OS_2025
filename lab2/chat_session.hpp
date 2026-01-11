#pragma once
#include "common.hpp"
#include <string>
#include <memory>

class ChatSession {
private:
    std::unique_ptr<Conn> connection;
    std::string client_name;
    int client_id;
    bool is_host;
    std::string username;
    
public:
    ChatSession(std::unique_ptr<Conn> conn, int id, bool is_host_mode, const std::string& name);
    
    void sendWelcomeMessage();
    void sendDisconnectMessage();
    bool processIncomingMessage();
    void sendPublicMessage(const std::string& text);
    void sendPrivateMessage(const std::string& text, int recipient_id);
    void processConsoleInput(const std::string& input);
    bool waitForMessage();
    
    bool isRunning() const { return running; }
    void stop() { running = false; }
    
private:
    std::atomic<bool> running{true};
    
    void handlePublicMessage(const ChatMessage& msg);
    void handlePrivateMessage(const ChatMessage& msg);
    void handleConnectMessage(const ChatMessage& msg);
    void handleDisconnectMessage(const ChatMessage& msg);
    void handleUnknownMessage(const ChatMessage& msg);
};

