#pragma once
#include "common.hpp"
#include <string>
#include <memory>

void host_signal_handler(int sig);
void client_signal_handler(int sig);
void setupHostSignalHandlers();
void setupClientSignalHandlers();
sem_t* createGlobalSemaphore();
void cleanupResources(sem_t* semaphore);
std::unique_ptr<Conn> createConnection(const std::string& type, int id, bool create);
bool waitForClientConnection(Conn* connection);
std::string getConsoleInput(int timeout_ms = 100);
