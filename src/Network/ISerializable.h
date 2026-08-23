//
// Created by MINEC on 2026/5/8.
// SDL_net 迁移 N3：序列化契约切 eng::Packet（与第三方解耦）
//

#pragma once
#include "Packet.h"
#include "NetworkProtocol.h"

class ISerializable {
public:
    virtual ~ISerializable() = default;
    virtual void serialize(eng::Packet& packet, NetworkMsg type) = 0;
    virtual void deserialize(eng::Packet& packet) = 0;
    virtual unsigned int getNetworkId() = 0;
    virtual void disconnect() = 0;
};
