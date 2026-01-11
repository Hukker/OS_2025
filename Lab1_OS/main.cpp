#include "daemon.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string configPath = "daemon.conf";
    
    // Allow custom config path as command line argument
    if (argc > 1) {
        configPath = argv[1];
    }
    
    Daemon& daemon = Daemon::getInstance();
    
    if (!daemon.initialize(configPath)) {
        std::cerr << "Failed to initialize daemon" << std::endl;
        return EXIT_FAILURE;
    }
    
    daemon.run();
    
    return EXIT_SUCCESS;
}