#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Network {

// Packet types for netcode protocol
enum class PacketType : uint8_t {
    ConnectionRequest = 0x01,
    ConnectionAccept = 0x02,
    ConnectionDeny = 0x03,
    Disconnect = 0x04,
    Heartbeat = 0x05,
    GameData = 0x10,
    PlayerJoin = 0x11,
    PlayerLeave = 0x12,
    PlayerUpdate = 0x13
};

// Basic packet structure
struct Packet {
    PacketType type;
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t dataSize;
    std::vector<uint8_t> data;

    Packet(PacketType t = PacketType::Heartbeat);
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& buffer);
};

// Connection info
struct ConnectionInfo {
    std::string address;
    uint16_t port;
    uint32_t clientId;
    bool connected;
    uint32_t lastHeartbeat;

    ConnectionInfo();
    ConnectionInfo(const std::string& addr, uint16_t p, uint32_t id = 0);
};

// Network utilities
class NetworkUtils {
public:
    static uint32_t getCurrentTimestamp();
    static bool isValidAddress(const std::string& address);
    static uint16_t calculateChecksum(const std::vector<uint8_t>& data);
};

} // namespace Network