#pragma once

#include "network/NetworkManager.h"
#include "network/Protocol.h"
#include <map>
#include <vector>
#include <mutex>
#include <chrono>

namespace Server {

struct Player {
    uint32_t id;
    Network::ConnectionInfo connection;
    std::string name;
    float x, y; // Position
    std::chrono::steady_clock::time_point lastUpdate;
    
    Player(uint32_t playerId, const Network::ConnectionInfo& conn);
};

class GameServer : public Network::NetworkManager {
public:
    GameServer();
    ~GameServer();
    
    bool start(const std::string& address, uint16_t port);
    void stop();
    
    void updateGame();
    void broadcastToAllPlayers(const Network::Packet& packet);
    
    size_t getPlayerCount() const;
    bool isPlayerConnected(uint32_t playerId) const;
    
private:
    std::map<uint32_t, Player> players;
    mutable std::mutex playersMutex;
    uint32_t nextPlayerId;
    
    std::chrono::steady_clock::time_point lastHeartbeat;
    static constexpr std::chrono::milliseconds HEARTBEAT_INTERVAL{5000};
    static constexpr std::chrono::milliseconds CLIENT_TIMEOUT{15000};
    
    void handlePacket(const Network::Packet& packet, const Network::ConnectionInfo& from);
    void handleConnectionRequest(const Network::Packet& packet, const Network::ConnectionInfo& from);
    void handlePlayerUpdate(const Network::Packet& packet, const Network::ConnectionInfo& from);
    void handleDisconnect(const Network::Packet& packet, const Network::ConnectionInfo& from);
    
    void sendHeartbeats();
    void checkClientTimeouts();
    void removePlayer(uint32_t playerId);
    
    uint32_t findPlayerByConnection(const Network::ConnectionInfo& conn);
};

} // namespace Server