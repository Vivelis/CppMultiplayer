#include "server/GameServer.h"
#include <iostream>
#include <signal.h>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    std::cout << "=== C++ Multiplayer Game Server ===" << std::endl;
    
    // Parse command line arguments
    std::string address = "0.0.0.0";
    uint16_t port = 7777;
    
    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        address = argv[2];
    }
    
    std::cout << "Server configuration:" << std::endl;
    std::cout << "  Address: " << address << std::endl;
    std::cout << "  Port: " << port << std::endl;
    
    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Create and start server
    Server::GameServer server;
    
    if (!server.start(address, port)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }
    
    std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
    
    // Main game loop
    while (running && server.isRunning()) {
        server.updateGame();
        
        // Print status every 10 seconds
        static auto lastStatus = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatus >= std::chrono::seconds(10)) {
            std::cout << "Server status: " << server.getPlayerCount() << " players connected" << std::endl;
            lastStatus = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    server.stop();
    std::cout << "Server shutdown complete." << std::endl;
    
    return 0;
}