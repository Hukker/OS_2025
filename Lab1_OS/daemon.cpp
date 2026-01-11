#include "./daemon.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <filesystem>

//Daemon* Daemon::instance_ = nullptr;

Daemon& Daemon::getInstance() {
    static Daemon instance;
    return instance;
}

void signalHandler(int sig) {
    switch(sig) {
        case SIGHUP:
            Daemon::getInstance().reloadConfig();
            break;
        case SIGTERM:
            Daemon::getInstance().terminate();
            break;
    }
}

bool Daemon::initialize(const std::string& configPath) {
    configPath_ = configPath;
    pidFilePath_ = "/var/run/directory_daemon.pid";
    running_ = true;
    
    char absPath[PATH_MAX];
    if (realpath(configPath.c_str(), absPath) == nullptr) {
        std::cerr << "Error getting absolute path for config file" << std::endl;
        return false;
    }
    configPath_ = absPath;
    
    if (!checkExistingProcess()) {
        return false;
    }
    
    if (!daemonize()) {
        return false;
    }
    
    if (!createPidFile()) {
        return false;
    }
    
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // Ignore other signals that might interrupt system calls
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    
    if (!readConfig()) {
        return false;
    }
    
    logMessage(LOG_INFO, "Daemon initialized successfully");
    return true;
}

bool Daemon::daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return false;
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    umask(0);
    
    if (setsid() < 0) {
        std::cerr << "Failed to create new session" << std::endl;
        return false;
    }
    
    if ((chdir("/")) < 0) {
        std::cerr << "Failed to change working directory" << std::endl;
        return false;
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
    
    return true;
}

bool Daemon::checkExistingProcess() {
    std::ifstream pidFile(pidFilePath_);
    if (!pidFile.is_open()) {
        return true;
    }
    
    pid_t oldPid;
    pidFile >> oldPid;
    pidFile.close();
    
    std::string procPath = "/proc/" + std::to_string(oldPid);
    struct stat statBuf;
    if (stat(procPath.c_str(), &statBuf) == 0) {
        if (kill(oldPid, SIGTERM) == 0) {
            logMessage(LOG_INFO, "Sent SIGTERM to existing process " + std::to_string(oldPid));
            
            sleep(2);
        }
    }
    
    return true;
}

bool Daemon::createPidFile() {
    std::ofstream pidFile(pidFilePath_);
    if (!pidFile.is_open()) {
        logMessage(LOG_ERR, "Failed to create PID file: " + pidFilePath_);
        return false;
    }
    
    pidFile << getpid() << std::endl;
    pidFile.close();
    
    return true;
}

bool Daemon::readConfig() {
    std::ifstream configFile(configPath_);
    if (!configFile.is_open()) {
        logMessage(LOG_ERR, "Failed to open config file: " + configPath_);
        return false;
    }
    
    std::string line;
    directories_.clear();
    directoryDepths_.clear();
    
    while (std::getline(configFile, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string key, value;
        
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "interval") {
                interval_ = std::stoi(value);
            } else if (key == "directory") {
                directories_.push_back(value);
            } else if (key.find("depth") != std::string::npos) {
                size_t pos = key.find('_');
                if (pos != std::string::npos) {
                    std::string dirName = key.substr(pos + 1);
                    directoryDepths_[dirName] = std::stoi(value);
                }
            }
        }
    }
    
    configFile.close();
    
    if (directories_.empty()) {
        logMessage(LOG_ERR, "No directories specified in config file");
        return false;
    }
    
    if (interval_ <= 0) {
        interval_ = 60; // Default to 60 seconds
    }
    
    logMessage(LOG_INFO, "Configuration loaded: interval=" + std::to_string(interval_) + 
               "s, directories=" + std::to_string(directories_.size()));
    
    return true;
}

void Daemon::run() {
    logMessage(LOG_INFO, "Daemon started running");
    
    while (running_) {
        performActions();
        
        for (int i = 0; i < interval_ && running_; i++) {
            sleep(1);
        }
    }
}

void Daemon::performActions() {
    for (const auto& dir : directories_) {
        auto it = directoryDepths_.find(dir);
        int depth = (it != directoryDepths_.end()) ? it->second : 1;
        
        logMessage(LOG_INFO, "Processing directory: " + dir + " with depth: " + std::to_string(depth));
        removeSubdirectories(dir, depth);
    }
}

void Daemon::removeSubdirectories(const std::string& path, int depth) {
    if (depth <= 0) return;
    
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        logMessage(LOG_ERR, "Cannot open directory: " + path);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        
        struct stat statBuf;
        if (stat(fullPath.c_str(), &statBuf) == 0) {
            if (S_ISDIR(statBuf.st_mode)) {
                removeSubdirectories(fullPath, depth - 1);
                
                if (rmdir(fullPath.c_str()) == 0) {
                    logMessage(LOG_INFO, "Removed directory: " + fullPath);
                } else {
                    logMessage(LOG_ERR, "Failed to remove directory: " + fullPath);
                }
            }
        }
    }
    
    closedir(dir);
}

void Daemon::reloadConfig() {
    logMessage(LOG_INFO, "Reloading configuration");
    readConfig();
}

void Daemon::terminate() {
    logMessage(LOG_INFO, "Received termination signal, shutting down");
    running_ = false;
    
    // Remove PID file
    if (remove(pidFilePath_.c_str()) != 0) {
        logMessage(LOG_ERR, "Failed to remove PID file");
    }
    
    closelog();
    exit(EXIT_SUCCESS);
}

void Daemon::stop() {
    terminate();
}

void Daemon::logMessage(int priority, const std::string& message) {
    openlog("directory_daemon", LOG_PID, LOG_DAEMON);
    syslog(priority, "%s", message.c_str());
    closelog();
}
