#include "chat_session.hpp"
#include <iostream>

ChatSession::ChatSession(std::unique_ptr<Conn> conn, int id, bool is_host_mode, const std::string& name)
    : connection(std::move(conn)), client_id(id), is_host(is_host_mode), username(name) {
    
    client_name = (is_host ? "Host" : "Client_") + std::to_string(client_id);
    Log(client_name + " сессия создана");
}

void ChatSession::sendWelcomeMessage() {
    ChatMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_CONNECT;
    msg.sender_id = is_host ? 0 : client_id;
    msg.recipient_id = is_host ? client_id : 0;
    strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
    strncpy(msg.text, "Добро пожаловать в чат!", sizeof(msg.text) - 1);
    msg.timestamp = time(nullptr);
    
    if (connection->Write(&msg, sizeof(msg))) {
        Log("Приветственное сообщение отправлено");
    } else {
        Log("Ошибка отправки приветственного сообщения");
    }
}

void ChatSession::sendDisconnectMessage() {
    ChatMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_DISCONNECT;
    msg.sender_id = client_id;
    strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
    strncpy(msg.text, "покинул чат", sizeof(msg.text) - 1);
    msg.timestamp = time(nullptr);
    
    connection->Write(&msg, sizeof(msg));
}

void ChatSession::handlePublicMessage(const ChatMessage& msg) {
    Log("[" + std::string(msg.username) + "]: " + msg.text);
    
    if (is_host) {
        // Эхо-ответ от хоста
        ChatMessage echo_msg;
        memset(&echo_msg, 0, sizeof(echo_msg));
        echo_msg.type = MSG_PUBLIC;
        echo_msg.sender_id = 0;
        echo_msg.recipient_id = client_id;
        strcpy(echo_msg.username, "Host");
        
        std::string response = "Получил: ";
        response += msg.text;
        strncpy(echo_msg.text, response.c_str(), sizeof(echo_msg.text) - 1);
        echo_msg.timestamp = time(nullptr);
        
        connection->Write(&echo_msg, sizeof(echo_msg));
    }
}

void ChatSession::handlePrivateMessage(const ChatMessage& msg) {
    Log("Личное от " + std::string(msg.username) + 
        " для " + std::to_string(msg.recipient_id) + 
        ": " + msg.text);
}

void ChatSession::handleConnectMessage(const ChatMessage& msg) {
    Log(std::string(msg.username) + " присоединился к чату");
    
    if (!is_host) {
        std::cout << "\n=== " << msg.text << " ===\n" << std::endl;
        std::cout << "> " << std::flush;
    }
}

void ChatSession::handleDisconnectMessage(const ChatMessage& msg) {
    Log(std::string(msg.username) + " покинул чат");
    
    if (!is_host) {
        std::cout << "\nСервер отключился" << std::endl;
    }
    
    running = false;
}

void ChatSession::handleUnknownMessage(const ChatMessage& msg) {
    Log("Неизвестный тип сообщения: " + std::to_string(msg.type));
}

bool ChatSession::processIncomingMessage() {
    ChatMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    if (!connection->Read(&msg, sizeof(msg))) {
        return false;
    }
    
    msg.timestamp = time(nullptr);
    
    // Обработка в зависимости от типа сообщения
    switch (msg.type) {
        case MSG_PUBLIC:
            handlePublicMessage(msg);
            break;
        case MSG_PRIVATE:
            handlePrivateMessage(msg);
            break;
        case MSG_CONNECT:
            handleConnectMessage(msg);
            break;
        case MSG_DISCONNECT:
            handleDisconnectMessage(msg);
            break;
        default:
            handleUnknownMessage(msg);
            break;
    }
    
    return true;
}

void ChatSession::sendPublicMessage(const std::string& text) {
    ChatMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_PUBLIC;
    msg.sender_id = client_id;
    msg.recipient_id = 0;
    strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
    strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
    msg.timestamp = time(nullptr);
    
    if (connection->Write(&msg, sizeof(msg))) {
        Log("[" + username + "]: " + text);
    } else {
        Log("Ошибка отправки сообщения");
    }
}

void ChatSession::sendPrivateMessage(const std::string& text, int recipient_id) {
    ChatMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_PRIVATE;
    msg.sender_id = client_id;
    msg.recipient_id = recipient_id;
    strncpy(msg.username, username.c_str(), sizeof(msg.username) - 1);
    strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
    msg.timestamp = time(nullptr);
    
    connection->Write(&msg, sizeof(msg));
}

void ChatSession::processConsoleInput(const std::string& input) {
    if (input.empty()) return;
    
    if (input == "/exit") {
        running = false;
        return;
    }
    
    if (input == "/help") {
        std::cout << "Команды:\n";
        std::cout << "  /exit - выход\n";
        std::cout << "  /help - справка\n";
        std::cout << "  @<id> <текст> - личное сообщение\n";
        std::cout << "  <текст> - публичное сообщение\n";
        return;
    }
    
    if (input[0] == '@') {
        size_t space_pos = input.find(' ');
        if (space_pos != std::string::npos) {
            std::string recipient = input.substr(1, space_pos - 1);
            std::string message = input.substr(space_pos + 1);
            sendPrivateMessage(message, std::stoi(recipient));
        }
    } else {
        sendPublicMessage(input);
    }
}

bool ChatSession::waitForMessage() {
    usleep(100000); 
    return running;
}