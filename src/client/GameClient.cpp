#include "client/GameClient.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

namespace Client {

RemotePlayer::RemotePlayer(uint32_t playerId) 
    : id(playerId), x(0.0f), y(0.0f), lastUpdate(std::chrono::steady_clock::now()) {
    name = "Player" + std::to_string(playerId);
}

GameClient::GameClient() 
    : state(ClientState::Disconnected), playerId(0), playerX(0.0f), playerY(0.0f),
      lastHeartbeat(std::chrono::steady_clock::now()), lastUpdate(std::chrono::steady_clock::now()) {
    
    setPacketHandler([this](const Network::Packet& packet, const Network::ConnectionInfo& from) {
        handlePacket(packet, from);
    });
}

GameClient::~GameClient() {
    disconnect();
}

bool GameClient::connect(const std::string& serverAddress, uint16_t serverPort) {
    if (state != ClientState::Disconnected) {
        return false;
    }
    
    std::cout << "Connecting to server " << serverAddress << ":" << serverPort << std::endl;
    
    serverConnection = Network::ConnectionInfo(serverAddress, serverPort);
    
    if (!initialize("0.0.0.0", 0)) { // Bind to any available port
        std::cerr << "Failed to initialize client network" << std::endl;
        return false;
    }
    
    state = ClientState::Connecting;
    sendConnectionRequest();
    
    return true;
}

void GameClient::disconnect() {
    if (state == ClientState::Disconnected) {
        return;
    }
    
    std::cout << "Disconnecting from server..." << std::endl;
    
    // Send disconnect packet
    Network::Packet disconnectPacket(Network::PacketType::Disconnect);
    sendPacket(disconnectPacket, serverConnection);
    
    state = ClientState::Disconnected;
    shutdown();
    
    remotePlayers.clear();
    playerId = 0;
    
    if (onDisconnected) {
        onDisconnected();
    }
}

void GameClient::update() {
    if (state == ClientState::Disconnected) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    
    // Send heartbeat periodically
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        lastHeartbeat = now;
    }
    
    // Send position updates
    if (state == ClientState::InGame && now - lastUpdate >= UPDATE_INTERVAL) {
        sendPlayerUpdate(playerX, playerY);
        lastUpdate = now;
    }
    
    // Process network updates
    NetworkManager::update();
}

void GameClient::sendPlayerUpdate(float x, float y) {
    if (state != ClientState::InGame) {
        return;
    }
    
    playerX = x;
    playerY = y;
    
    Network::Packet updatePacket(Network::PacketType::PlayerUpdate);
    updatePacket.data.resize(8);
    std::memcpy(updatePacket.data.data(), &x, 4);
    std::memcpy(updatePacket.data.data() + 4, &y, 4);
    updatePacket.dataSize = 8;
    
    sendPacket(updatePacket, serverConnection);
}

ClientState GameClient::getState() const {
    return state;
}

uint32_t GameClient::getPlayerId() const {
    return playerId;
}

size_t GameClient::getRemotePlayerCount() const {
    return remotePlayers.size();
}

void GameClient::setPosition(float x, float y) {
    playerX = x;
    playerY = y;
}

std::pair<float, float> GameClient::getPosition() const {
    return {playerX, playerY};
}

void GameClient::setOnConnected(std::function<void()> callback) {
    onConnected = std::move(callback);
}

void GameClient::setOnDisconnected(std::function<void()> callback) {
    onDisconnected = std::move(callback);
}

void GameClient::setOnPlayerJoined(std::function<void(uint32_t)> callback) {
    onPlayerJoined = std::move(callback);
}

void GameClient::setOnPlayerLeft(std::function<void(uint32_t)> callback) {
    onPlayerLeft = std::move(callback);
}

void GameClient::setOnPlayerUpdate(std::function<void(uint32_t, float, float)> callback) {
    onPlayerUpdate = std::move(callback);
}

void GameClient::handlePacket(const Network::Packet& packet, const Network::ConnectionInfo& from) {
    // Verify packet is from server
    if (from.address != serverConnection.address || from.port != serverConnection.port) {
        return;
    }
    
    switch (packet.type) {
        case Network::PacketType::ConnectionAccept:
            handleConnectionAccept(packet);
            break;
        case Network::PacketType::ConnectionDeny:
            handleConnectionDeny(packet);
            break;
        case Network::PacketType::PlayerJoin:
            handlePlayerJoin(packet);
            break;
        case Network::PacketType::PlayerLeave:
            handlePlayerLeave(packet);
            break;
        case Network::PacketType::PlayerUpdate:
            handlePlayerUpdate(packet);
            break;
        case Network::PacketType::Disconnect:
            handleDisconnect(packet);
            break;
        case Network::PacketType::Heartbeat:
            // Server heartbeat received, connection is alive
            break;
        default:
            std::cout << "Unknown packet type received: " << static_cast<int>(packet.type) << std::endl;
            break;
    }
}

void GameClient::handleConnectionAccept(const Network::Packet& packet) {
    if (packet.data.size() >= 4) {
        uint32_t netPlayerId;
        std::memcpy(&netPlayerId, packet.data.data(), 4);
        playerId = ntohl(netPlayerId);
        
        state = ClientState::InGame;
        std::cout << "Connected to server! Player ID: " << playerId << std::endl;
        
        if (onConnected) {
            onConnected();
        }
    }
}

void GameClient::handleConnectionDeny(const Network::Packet& packet) {
    std::cout << "Connection denied by server" << std::endl;
    state = ClientState::Disconnected;
    
    if (onDisconnected) {
        onDisconnected();
    }
}

void GameClient::handlePlayerJoin(const Network::Packet& packet) {
    if (packet.data.size() >= 4) {
        uint32_t netPlayerId;
        std::memcpy(&netPlayerId, packet.data.data(), 4);
        uint32_t joinedPlayerId = ntohl(netPlayerId);
        
        if (joinedPlayerId != playerId) {
            remotePlayers.emplace(joinedPlayerId, RemotePlayer(joinedPlayerId));
            std::cout << "Player " << joinedPlayerId << " joined the game" << std::endl;
            
            if (onPlayerJoined) {
                onPlayerJoined(joinedPlayerId);
            }
        }
    }
}

void GameClient::handlePlayerLeave(const Network::Packet& packet) {
    if (packet.data.size() >= 4) {
        uint32_t netPlayerId;
        std::memcpy(&netPlayerId, packet.data.data(), 4);
        uint32_t leftPlayerId = ntohl(netPlayerId);
        
        auto it = remotePlayers.find(leftPlayerId);
        if (it != remotePlayers.end()) {
            remotePlayers.erase(it);
            std::cout << "Player " << leftPlayerId << " left the game" << std::endl;
            
            if (onPlayerLeft) {
                onPlayerLeft(leftPlayerId);
            }
        }
    }
}

void GameClient::handlePlayerUpdate(const Network::Packet& packet) {
    if (packet.data.size() >= 12) {
        uint32_t netPlayerId;
        float x, y;
        
        std::memcpy(&netPlayerId, packet.data.data(), 4);
        std::memcpy(&x, packet.data.data() + 4, 4);
        std::memcpy(&y, packet.data.data() + 8, 4);
        
        uint32_t updatedPlayerId = ntohl(netPlayerId);
        
        auto it = remotePlayers.find(updatedPlayerId);
        if (it != remotePlayers.end()) {
            it->second.x = x;
            it->second.y = y;
            it->second.lastUpdate = std::chrono::steady_clock::now();
            
            if (onPlayerUpdate) {
                onPlayerUpdate(updatedPlayerId, x, y);
            }
        }
    }
}

void GameClient::handleDisconnect(const Network::Packet& packet) {
    std::cout << "Disconnected by server" << std::endl;
    state = ClientState::Disconnected;
    
    if (onDisconnected) {
        onDisconnected();
    }
}

void GameClient::sendHeartbeat() {
    Network::Packet heartbeat(Network::PacketType::Heartbeat);
    sendPacket(heartbeat, serverConnection);
}

void GameClient::sendConnectionRequest() {
    Network::Packet request(Network::PacketType::ConnectionRequest);
    sendPacket(request, serverConnection);
}

} // namespace Client