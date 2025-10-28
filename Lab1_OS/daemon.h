#ifndef DAEMON_H
#define DAEMON_H

#include <string>
#include <vector>
#include <unordered_map>

class Daemon {
public:
    static Daemon& getInstance();
    
    bool initialize(const std::string& configPath);
    void run();
    void stop();
    
    // Signal handlers
    void reloadConfig();
    void terminate();

private:
    Daemon() = default;
    ~Daemon() = default;
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;
    
    bool daemonize();
    bool createPidFile();
    bool checkExistingProcess();
    bool readConfig();
    void performActions();
    void removeSubdirectories(const std::string& path, int depth);
    void logMessage(int priority, const std::string& message);
    
    std::string configPath_;
    std::string pidFilePath_;
    int interval_;
    std::vector<std::string> directories_;
    std::unordered_map<std::string, int> directoryDepths_;
    bool running_;
    static Daemon* instance_;
};

#endif