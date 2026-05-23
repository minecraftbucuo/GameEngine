//
// Created by MINEC on 2026/5/8.
//

#pragma once
#include <cstdint>

// 1. 发送：通用枚举重载
template <typename T>
std::enable_if_t<std::is_enum_v<T>, sf::Packet&>
operator <<(sf::Packet& packet, const T& enumVal) {
    // 使用 std::underlying_type 获取枚举的实际底层类型（如 uint8_t 或 int）
    using Underlying = std::underlying_type_t<T>;
    return packet << static_cast<Underlying>(enumVal);
}

// 2. 接收：通用枚举重载
template <typename T>
std::enable_if_t<std::is_enum_v<T>, sf::Packet&>
operator >>(sf::Packet& packet, T& enumVal) {
    using Underlying = std::underlying_type_t<T>;
    Underlying val;
    packet >> val;
    enumVal = static_cast<T>(val);
    return packet;
}

enum class NetworkMsg : uint8_t {
    SpawnPlayer = 0,
    SpawnObject = 1,
    UpdateObject = 2,
    RemoveObject = 3,
    ClientInput = 4,
    SpawnFireBall = 5,
    ClientRespawn = 6
};

enum class ObjectType : uint8_t {
    MarioPlayer = 0,
    Mario = 1,
    CircleObject = 2, // 暂不维护
    BoxGameObject = 3, // 暂不维护
    FireBall = 4
};

enum class InputType : uint8_t {
    Jump = 0,
    RunLeft = 1,
    RunRight = 2,
    StopRunLeft = 3,
    StopRunRight = 4,
    JumpRelease = 5,
    Shoot = 6
};