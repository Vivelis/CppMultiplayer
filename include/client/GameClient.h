#pragma once

#include "network/NetworkManager.h"
#include "network/Protocol.h"
#include <map>
#include <chrono>
#include <atomic>

namespace Client {

struct RemotePlayer {
    uint32_t id;
    std::string name;
    float x, y;
    std::chrono::steady_clock::time_point lastUpdate;
    
    RemotePlayer(uint32_t playerId);
};

enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
    InGame
};

class GameClient : public Network::NetworkManager {
public:
    GameClient();
    ~GameClient();
    
    bool connect(const std::string& serverAddress, uint16_t serverPort);
    void disconnect();
    
    void update();
    void sendPlayerUpdate(float x, float y);
    
    ClientState getState() const;
    uint32_t getPlayerId() const;
    size_t getRemotePlayerCount() const;
    
    void setPosition(float x, float y);
    std::pair<float, float> getPosition() const;
    
    // Event callbacks
    void setOnConnected(std::function<void()> callback);
    void setOnDisconnected(std::function<void()> callback);
    void setOnPlayerJoined(std::function<void(uint32_t)> callback);
    void setOnPlayerLeft(std::function<void(uint32_t)> callback);
    void setOnPlayerUpdate(std::function<void(uint32_t, float, float)> callback);
    
private:
    Network::ConnectionInfo serverConnection;
    std::map<uint32_t, RemotePlayer> remotePlayers;
    
    std::atomic<ClientState> state;
    uint32_t playerId;
    float playerX, playerY;
    
    std::chrono::steady_clock::time_point lastHeartbeat;
    std::chrono::steady_clock::time_point lastUpdate;
    static constexpr std::chrono::milliseconds HEARTBEAT_INTERVAL{2000};
    static constexpr std::chrono::milliseconds UPDATE_INTERVAL{50}; // 20 FPS
    
    // Event callbacks
    std::function<void()> onConnected;
    std::function<void()> onDisconnected;
    std::function<void(uint32_t)> onPlayerJoined;
    std::function<void(uint32_t)> onPlayerLeft;
    std::function<void(uint32_t, float, float)> onPlayerUpdate;
    
    void handlePacket(const Network::Packet& packet, const Network::ConnectionInfo& from);
    void handleConnectionAccept(const Network::Packet& packet);
    void handleConnectionDeny(const Network::Packet& packet);
    void handlePlayerJoin(const Network::Packet& packet);
    void handlePlayerLeave(const Network::Packet& packet);
    void handlePlayerUpdate(const Network::Packet& packet);
    void handleDisconnect(const Network::Packet& packet);
    
    void sendHeartbeat();
    void sendConnectionRequest();
};

} // namespace Client