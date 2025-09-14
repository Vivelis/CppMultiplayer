#include "server/GameServer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>

namespace Server {

Player::Player(uint32_t playerId, const Network::ConnectionInfo& conn)
    : id(playerId), connection(conn), x(0.0f), y(0.0f), lastUpdate(std::chrono::steady_clock::now()) {
    name = "Player" + std::to_string(playerId);
}

GameServer::GameServer() : nextPlayerId(1), lastHeartbeat(std::chrono::steady_clock::now()) {
    setPacketHandler([this](const Network::Packet& packet, const Network::ConnectionInfo& from) {
        handlePacket(packet, from);
    });
}

GameServer::~GameServer() {
    stop();
}

bool GameServer::start(const std::string& address, uint16_t port) {
    std::cout << "Starting game server on " << address << ":" << port << std::endl;
    
    if (!initialize(address, port)) {
        std::cerr << "Failed to initialize server" << std::endl;
        return false;
    }
    
    std::cout << "Game server started successfully!" << std::endl;
    return true;
}

void GameServer::stop() {
    std::cout << "Stopping game server..." << std::endl;
    
    // Send disconnect to all players
    Network::Packet disconnectPacket(Network::PacketType::Disconnect);
    broadcastToAllPlayers(disconnectPacket);
    
    shutdown();
    
    std::lock_guard<std::mutex> lock(playersMutex);
    players.clear();
    
    std::cout << "Game server stopped." << std::endl;
}

void GameServer::updateGame() {
    auto now = std::chrono::steady_clock::now();
    
    // Send heartbeats periodically
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        sendHeartbeats();
        lastHeartbeat = now;
    }
    
    // Check for client timeouts
    checkClientTimeouts();
    
    // Process network updates
    update();
}

void GameServer::broadcastToAllPlayers(const Network::Packet& packet) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    for (auto& [playerId, player] : players) {
        sendPacket(packet, player.connection);
    }
}

size_t GameServer::getPlayerCount() const {
    std::lock_guard<std::mutex> lock(playersMutex);
    return players.size();
}

bool GameServer::isPlayerConnected(uint32_t playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    return players.find(playerId) != players.end();
}

void GameServer::handlePacket(const Network::Packet& packet, const Network::ConnectionInfo& from) {
    switch (packet.type) {
        case Network::PacketType::ConnectionRequest:
            handleConnectionRequest(packet, from);
            break;
        case Network::PacketType::PlayerUpdate:
            handlePlayerUpdate(packet, from);
            break;
        case Network::PacketType::Disconnect:
            handleDisconnect(packet, from);
            break;
        case Network::PacketType::Heartbeat:
            // Update last heartbeat for the player
            {
                uint32_t playerId = findPlayerByConnection(from);
                if (playerId != 0) {
                    std::lock_guard<std::mutex> lock(playersMutex);
                    auto it = players.find(playerId);
                    if (it != players.end()) {
                        it->second.lastUpdate = std::chrono::steady_clock::now();
                    }
                }
            }
            break;
        default:
            std::cout << "Unknown packet type received: " << static_cast<int>(packet.type) << std::endl;
            break;
    }
}

void GameServer::handleConnectionRequest(const Network::Packet& packet, const Network::ConnectionInfo& from) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    // Check if player is already connected
    uint32_t existingId = findPlayerByConnection(from);
    if (existingId != 0) {
        std::cout << "Player already connected from " << from.address << ":" << from.port << std::endl;
        return;
    }
    
    // Create new player
    uint32_t playerId = nextPlayerId++;
    Network::ConnectionInfo playerConn = from;
    playerConn.clientId = playerId;
    playerConn.connected = true;
    
    players.emplace(playerId, Player(playerId, playerConn));
    
    std::cout << "Player " << playerId << " connected from " << from.address << ":" << from.port << std::endl;
    
    // Send connection acceptance
    Network::Packet response(Network::PacketType::ConnectionAccept);
    response.data.resize(4);
    uint32_t netPlayerId = htonl(playerId);
    std::memcpy(response.data.data(), &netPlayerId, 4);
    response.dataSize = 4;
    
    sendPacket(response, playerConn);
    
    // Notify other players
    Network::Packet joinPacket(Network::PacketType::PlayerJoin);
    joinPacket.data.resize(4);
    std::memcpy(joinPacket.data.data(), &netPlayerId, 4);
    joinPacket.dataSize = 4;
    
    for (auto& [id, player] : players) {
        if (id != playerId) {
            sendPacket(joinPacket, player.connection);
        }
    }
}

void GameServer::handlePlayerUpdate(const Network::Packet& packet, const Network::ConnectionInfo& from) {
    uint32_t playerId = findPlayerByConnection(from);
    if (playerId == 0) return;
    
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end()) return;
    
    // Update player position (assuming packet contains x, y coordinates)
    if (packet.data.size() >= 8) {
        float x, y;
        std::memcpy(&x, packet.data.data(), 4);
        std::memcpy(&y, packet.data.data() + 4, 4);
        
        it->second.x = x;
        it->second.y = y;
        it->second.lastUpdate = std::chrono::steady_clock::now();
        
        // Broadcast update to other players
        Network::Packet updatePacket(Network::PacketType::PlayerUpdate);
        updatePacket.data.resize(12); // playerId + x + y
        uint32_t netPlayerId = htonl(playerId);
        std::memcpy(updatePacket.data.data(), &netPlayerId, 4);
        std::memcpy(updatePacket.data.data() + 4, &x, 4);
        std::memcpy(updatePacket.data.data() + 8, &y, 4);
        updatePacket.dataSize = 12;
        
        for (auto& [id, player] : players) {
            if (id != playerId) {
                sendPacket(updatePacket, player.connection);
            }
        }
    }
}

void GameServer::handleDisconnect(const Network::Packet& packet, const Network::ConnectionInfo& from) {
    uint32_t playerId = findPlayerByConnection(from);
    if (playerId != 0) {
        removePlayer(playerId);
    }
}

void GameServer::sendHeartbeats() {
    Network::Packet heartbeat(Network::PacketType::Heartbeat);
    broadcastToAllPlayers(heartbeat);
}

void GameServer::checkClientTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> timeoutPlayers;
    
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        for (auto& [playerId, player] : players) {
            if (now - player.lastUpdate >= CLIENT_TIMEOUT) {
                timeoutPlayers.push_back(playerId);
            }
        }
    }
    
    for (uint32_t playerId : timeoutPlayers) {
        std::cout << "Player " << playerId << " timed out" << std::endl;
        removePlayer(playerId);
    }
}

void GameServer::removePlayer(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    auto it = players.find(playerId);
    if (it == players.end()) return;
    
    std::cout << "Player " << playerId << " disconnected" << std::endl;
    
    // Notify other players
    Network::Packet leavePacket(Network::PacketType::PlayerLeave);
    leavePacket.data.resize(4);
    uint32_t netPlayerId = htonl(playerId);
    std::memcpy(leavePacket.data.data(), &netPlayerId, 4);
    leavePacket.dataSize = 4;
    
    for (auto& [id, player] : players) {
        if (id != playerId) {
            sendPacket(leavePacket, player.connection);
        }
    }
    
    players.erase(it);
}

uint32_t GameServer::findPlayerByConnection(const Network::ConnectionInfo& conn) {
    for (auto& [playerId, player] : players) {
        if (player.connection.address == conn.address && player.connection.port == conn.port) {
            return playerId;
        }
    }
    return 0;
}

} // namespace Server