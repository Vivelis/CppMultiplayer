#include "network/Protocol.h"
#include <chrono>
#include <cstring>
#include <arpa/inet.h>

namespace Network {

Packet::Packet(PacketType t) 
    : type(t), sequence(0), timestamp(NetworkUtils::getCurrentTimestamp()), dataSize(0) {
}

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> buffer;
    
    // Packet header: type(1) + sequence(4) + timestamp(4) + dataSize(2) = 11 bytes
    buffer.resize(11 + data.size());
    
    buffer[0] = static_cast<uint8_t>(type);
    
    // Convert to network byte order
    uint32_t netSequence = htonl(sequence);
    uint32_t netTimestamp = htonl(timestamp);
    uint16_t netDataSize = htons(dataSize);
    
    std::memcpy(&buffer[1], &netSequence, 4);
    std::memcpy(&buffer[5], &netTimestamp, 4);
    std::memcpy(&buffer[9], &netDataSize, 2);
    
    if (!data.empty()) {
        std::memcpy(&buffer[11], data.data(), data.size());
    }
    
    return buffer;
}

bool Packet::deserialize(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 11) {
        return false;
    }
    
    type = static_cast<PacketType>(buffer[0]);
    
    uint32_t netSequence, netTimestamp;
    uint16_t netDataSize;
    
    std::memcpy(&netSequence, &buffer[1], 4);
    std::memcpy(&netTimestamp, &buffer[5], 4);
    std::memcpy(&netDataSize, &buffer[9], 2);
    
    // Convert from network byte order
    sequence = ntohl(netSequence);
    timestamp = ntohl(netTimestamp);
    dataSize = ntohs(netDataSize);
    
    if (buffer.size() < static_cast<size_t>(11 + dataSize)) {
        return false;
    }
    
    data.clear();
    if (dataSize > 0) {
        data.resize(dataSize);
        std::memcpy(data.data(), &buffer[11], dataSize);
    }
    
    return true;
}

ConnectionInfo::ConnectionInfo() 
    : port(0), clientId(0), connected(false), lastHeartbeat(0) {
}

ConnectionInfo::ConnectionInfo(const std::string& addr, uint16_t p, uint32_t id)
    : address(addr), port(p), clientId(id), connected(false), lastHeartbeat(NetworkUtils::getCurrentTimestamp()) {
}

uint32_t NetworkUtils::getCurrentTimestamp() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool NetworkUtils::isValidAddress(const std::string& address) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, address.c_str(), &(sa.sin_addr));
    return result != 0;
}

uint16_t NetworkUtils::calculateChecksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    for (size_t i = 0; i < data.size(); i += 2) {
        uint16_t word = data[i];
        if (i + 1 < data.size()) {
            word |= (data[i + 1] << 8);
        }
        sum += word;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

} // namespace Network