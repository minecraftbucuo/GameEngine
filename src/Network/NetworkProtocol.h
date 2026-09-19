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
    ClientRespawn = 6,
    ClientDeath = 7,   // 客户端本地判死上报：伤害判定为客户端预测，服务端据此移除权威对象
    ClientEvent = 8    // 方案 B：客户端预测交互结果后上报（踩扁/炸飞小怪），服务端落到权威对象并广播移除
};

// ClientEvent 的事件类型（附加数据见注释，服务端按类型读取）
enum class GameEventType : uint8_t {
    GoombaSquashed = 0,          // 踩扁小怪，无附加数据
    GoombaKilledByFireball = 1   // 炮弹击毙小怪，附加数据：float 炸飞方向（±1）
};

enum class ObjectType : uint8_t {
    MarioPlayer = 0,
    Mario = 1,
    CircleObject = 2, // 暂不维护
    BoxGameObject = 3, // 暂不维护
    FireBall = 4,
    Goomba = 5
};

enum class InputType : uint8_t {
    Jump = 0,
    RunLeft = 1,
    RunRight = 2,
    StopRun = 3,
    JumpRelease = 4,
    Shoot = 5
};
