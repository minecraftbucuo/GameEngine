//
// Created by MINEC on 2026/5/8.
// SDL_net 迁移 N3：枚举 operator<</>> 已内置于 eng::Packet（Packet.h 模板），
// 原 sf::Packet 版枚举模板删除；此处只保留协议消息定义。
//

#pragma once
#include <cstdint>
#include "Packet.h"

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
    StopRun = 3,
    JumpRelease = 4,
    Shoot = 5
};
