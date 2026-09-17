//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "NetworkGameObject.h"
#include "Events.h"
#include "Core/Types.h"

class Mario : public NetworkGameObject {
public:
    Mario(float x, float y, bool isPlayer = true);

    ~Mario() override;

    void start() override;

    void handleEvent(const eng::EngineEvent& e) override;

    void update(eng::Time deltaTime) override;

    bool needGravity();

    void handleCollision(const CollisionEvent& event);

    bool getIsPlayer() const;

    // 是否处于被击退状态（击退未结束前，按住的方向键不夺回方向）
    bool isKnockbackActive() const { return knock_time_left > 0.f; }

    void destroy() override;

    eng::Vec2f getCenter() override;

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

private:
    // 客户端本地玩家的服务端校正：小误差忽略，中等误差软修正，大误差直接同步。
    void reconcileLocalPlayer(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, bool isJump);

    // 直接应用服务端权威状态：本地的远端玩家直接同步服务端状态，避免碰撞上的bug。
    void setAuthoritativeState(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, bool isJump);

    bool isPlayer = true;
    // 被击退中的初速度与剩余时长：固定短时长匀减速，时间到恰好停住、交还控制权
    float knock_speed_x = 0.f;
    float knock_time_left = 0.f;
};
