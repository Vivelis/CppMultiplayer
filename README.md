# CppMultiplayer

A realtime multiplayer game implemented in C++ with low-level networking using a custom netcode protocol.

## Project Structure

- **src/network/**: Network library with custom netcode protocol
- **src/server/**: Game server implementation  
- **src/client/**: Game client implementation
- **include/**: Header files for all components

## Building

Requirements:
- CMake 3.16 or higher
- C++17 compatible compiler (GCC, Clang, etc.)
- POSIX-compliant system (Linux, macOS)

Build steps:
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Running

Start the server:
```bash
./build/src/server/server [port] [address]
# Default: ./build/src/server/server 7777 0.0.0.0
```

Connect clients:
```bash
./build/src/client/client [server_address] [server_port]
# Default: ./build/src/client/client 127.0.0.1 7777
```

## Features

- **Custom netcode protocol** with packet serialization/deserialization
- **Low-level UDP networking** with non-blocking sockets
- **Real-time player synchronization** with position updates
- **Connection management** with heartbeats and timeouts
- **Multiplayer support** for multiple concurrent clients
- **Cross-platform networking** (Linux/Unix systems)

## Network Protocol

The network library implements a custom protocol with the following packet types:
- ConnectionRequest/Accept/Deny
- PlayerJoin/Leave/Update
- Heartbeat and Disconnect
- Game data synchronization

All packets include sequence numbers, timestamps, and checksums for reliability.