#include "client/GameClient.h"
#include <iostream>
#include <thread>
#include <random>
#include <signal.h>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    std::cout << "=== C++ Multiplayer Game Client ===" << std::endl;
    
    // Parse command line arguments
    std::string serverAddress = "127.0.0.1";
    uint16_t serverPort = 7777;
    
    if (argc >= 2) {
        serverAddress = argv[1];
    }
    if (argc >= 3) {
        serverPort = std::atoi(argv[2]);
    }
    
    std::cout << "Client configuration:" << std::endl;
    std::cout << "  Server Address: " << serverAddress << std::endl;
    std::cout << "  Server Port: " << serverPort << std::endl;
    
    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Create client
    Client::GameClient client;
    
    // Setup event handlers
    client.setOnConnected([]() {
        std::cout << "Successfully connected to server!" << std::endl;
    });
    
    client.setOnDisconnected([]() {
        std::cout << "Disconnected from server!" << std::endl;
    });
    
    client.setOnPlayerJoined([](uint32_t playerId) {
        std::cout << "Player " << playerId << " joined the game" << std::endl;
    });
    
    client.setOnPlayerLeft([](uint32_t playerId) {
        std::cout << "Player " << playerId << " left the game" << std::endl;
    });
    
    client.setOnPlayerUpdate([](uint32_t playerId, float x, float y) {
        // Only print updates occasionally to avoid spam
        static auto lastPrint = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint >= std::chrono::seconds(5)) {
            std::cout << "Player " << playerId << " moved to (" << x << ", " << y << ")" << std::endl;
            lastPrint = now;
        }
    });
    
    // Connect to server
    if (!client.connect(serverAddress, serverPort)) {
        std::cerr << "Failed to connect to server!" << std::endl;
        return 1;
    }
    
    std::cout << "Client is running. Press Ctrl+C to stop." << std::endl;
    
    // Simulate player movement
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> speedDist(-5.0f, 5.0f);
    
    float x = posDist(gen);
    float y = posDist(gen);
    float velocityX = speedDist(gen);
    float velocityY = speedDist(gen);
    
    client.setPosition(x, y);
    
    auto lastMove = std::chrono::steady_clock::now();
    
    // Main game loop
    while (running && (client.getState() != Client::ClientState::Disconnected)) {
        client.update();
        
        // Simulate movement every 100ms
        auto now = std::chrono::steady_clock::now();
        if (now - lastMove >= std::chrono::milliseconds(100)) {
            if (client.getState() == Client::ClientState::InGame) {
                // Simple bouncing movement
                x += velocityX;
                y += velocityY;
                
                // Bounce off boundaries
                if (x <= -100.0f || x >= 100.0f) {
                    velocityX = -velocityX;
                    x = std::clamp(x, -100.0f, 100.0f);
                }
                if (y <= -100.0f || y >= 100.0f) {
                    velocityY = -velocityY;
                    y = std::clamp(y, -100.0f, 100.0f);
                }
                
                client.setPosition(x, y);
            }
            lastMove = now;
        }
        
        // Print status every 10 seconds
        static auto lastStatus = std::chrono::steady_clock::now();
        if (now - lastStatus >= std::chrono::seconds(10)) {
            if (client.getState() == Client::ClientState::InGame) {
                auto [posX, posY] = client.getPosition();
                std::cout << "Status: Player " << client.getPlayerId() 
                         << " at (" << posX << ", " << posY << "), "
                         << client.getRemotePlayerCount() << " other players" << std::endl;
            }
            lastStatus = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    client.disconnect();
    std::cout << "Client shutdown complete." << std::endl;
    
    return 0;
}