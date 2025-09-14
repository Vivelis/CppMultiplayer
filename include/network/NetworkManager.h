#pragma once

#include "Protocol.h"
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace Network {

class Socket {
public:
    Socket();
    ~Socket();
    
    bool bind(const std::string& address, uint16_t port);
    bool connect(const std::string& address, uint16_t port);
    void close();
    
    bool send(const std::vector<uint8_t>& data, const std::string& address = "", uint16_t port = 0);
    bool receive(std::vector<uint8_t>& data, std::string& fromAddress, uint16_t& fromPort);
    
    bool isValid() const;
    void setNonBlocking(bool nonBlocking);
    
private:
    int sockfd;
    bool valid;
    bool bound;
};

class NetworkManager {
public:
    using PacketHandler = std::function<void(const Packet&, const ConnectionInfo&)>;
    using ConnectionHandler = std::function<void(const ConnectionInfo&, bool connected)>;
    
    NetworkManager();
    virtual ~NetworkManager();
    
    bool initialize(const std::string& address, uint16_t port);
    void shutdown();
    
    void setPacketHandler(PacketHandler handler);
    void setConnectionHandler(ConnectionHandler handler);
    
    bool sendPacket(const Packet& packet, const ConnectionInfo& target);
    void update(); // Process incoming packets
    
    bool isRunning() const;
    
protected:
    std::unique_ptr<Socket> socket;
    PacketHandler packetHandler;
    ConnectionHandler connectionHandler;
    
    std::atomic<bool> running;
    std::thread networkThread;
    
    void networkLoop();
    void processIncomingData();
};

} // namespace Network