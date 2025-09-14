#include "network/NetworkManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace Network {

Socket::Socket() : sockfd(-1), valid(false), bound(false) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0) {
        valid = true;
    }
}

Socket::~Socket() {
    close();
}

bool Socket::bind(const std::string& address, uint16_t port) {
    if (!valid) return false;
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
    }
    
    if (::bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    
    bound = true;
    return true;
}

bool Socket::connect(const std::string& address, uint16_t port) {
    if (!valid) return false;
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        return false;
    }
    
    // For UDP, connect just sets the default destination
    return ::connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) >= 0;
}

void Socket::close() {
    if (valid && sockfd >= 0) {
        ::close(sockfd);
        sockfd = -1;
        valid = false;
        bound = false;
    }
}

bool Socket::send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    if (!valid || data.empty()) return false;
    
    if (address.empty() || port == 0) {
        // Use default destination (from connect)
        return ::send(sockfd, data.data(), data.size(), 0) == static_cast<ssize_t>(data.size());
    } else {
        // Send to specific address
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            return false;
        }
        
        return ::sendto(sockfd, data.data(), data.size(), 0, 
                       (struct sockaddr*)&addr, sizeof(addr)) == static_cast<ssize_t>(data.size());
    }
}

bool Socket::receive(std::vector<uint8_t>& data, std::string& fromAddress, uint16_t& fromPort) {
    if (!valid) return false;
    
    char buffer[4096];
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    
    ssize_t received = ::recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                                 (struct sockaddr*)&addr, &addrLen);
    
    if (received > 0) {
        data.resize(received);
        std::memcpy(data.data(), buffer, received);
        
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, addrStr, INET_ADDRSTRLEN);
        fromAddress = addrStr;
        fromPort = ntohs(addr.sin_port);
        
        return true;
    }
    
    return false;
}

bool Socket::isValid() const {
    return valid;
}

void Socket::setNonBlocking(bool nonBlocking) {
    if (!valid) return;
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (nonBlocking) {
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
    }
}

NetworkManager::NetworkManager() : running(false) {
    socket = std::make_unique<Socket>();
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize(const std::string& address, uint16_t port) {
    if (!socket->isValid()) {
        return false;
    }
    
    if (!socket->bind(address, port)) {
        return false;
    }
    
    socket->setNonBlocking(true);
    running = true;
    
    networkThread = std::thread(&NetworkManager::networkLoop, this);
    
    return true;
}

void NetworkManager::shutdown() {
    running = false;
    
    if (networkThread.joinable()) {
        networkThread.join();
    }
    
    if (socket) {
        socket->close();
    }
}

void NetworkManager::setPacketHandler(PacketHandler handler) {
    packetHandler = std::move(handler);
}

void NetworkManager::setConnectionHandler(ConnectionHandler handler) {
    connectionHandler = std::move(handler);
}

bool NetworkManager::sendPacket(const Packet& packet, const ConnectionInfo& target) {
    if (!socket->isValid() || !target.connected) {
        return false;
    }
    
    auto data = packet.serialize();
    return socket->send(data, target.address, target.port);
}

void NetworkManager::update() {
    // This method can be called from main thread for additional processing
    // Most work is done in the network thread
}

bool NetworkManager::isRunning() const {
    return running;
}

void NetworkManager::networkLoop() {
    while (running) {
        processIncomingData();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkManager::processIncomingData() {
    std::vector<uint8_t> data;
    std::string fromAddress;
    uint16_t fromPort;
    
    while (socket->receive(data, fromAddress, fromPort)) {
        Packet packet;
        if (packet.deserialize(data)) {
            ConnectionInfo connInfo(fromAddress, fromPort);
            connInfo.connected = true;
            
            if (packetHandler) {
                packetHandler(packet, connInfo);
            }
        }
    }
}

} // namespace Network