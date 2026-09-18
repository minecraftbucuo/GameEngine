//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "NetworkGameObject.h"
#include "Events.h"
#include "Timer.h"
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

    bool getIsBig() const;

    // 吃蘑菇长大：切换大马里奥贴图/动画、放大碰撞盒，长大后才能发射炮弹
    void growUp();

    // 受击缩小：切回小马里奥贴图/动画、原子缩小碰撞盒（脚底锚定），并进入变身闪烁
    void shrinkDown();

    // 变身闪烁中（原版变身过程：期间无敌、渲染在大小形态间交替）
    [[nodiscard]] bool isTransforming() const {
        return transforming;
    }

    // 闪烁相位：变身开始后每 100ms 递增一次，用于在两种形态间交替渲染
    [[nodiscard]] int getTransformPhase() const {
        return transform_timer.getPastTime() / 100;
    }

    void destroy() override;

    eng::Vec2f getCenter() override;

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

private:
    // 客户端本地玩家的服务端校正：小误差忽略，中等误差软修正，大误差直接同步。
    void reconcileLocalPlayer(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, bool isJump);

    // 直接应用服务端权威状态：本地的远端玩家直接同步服务端状态，避免碰撞上的bug。
    void setAuthoritativeState(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, bool isJump);

    // 受击统一入口：变身闪烁期间无敌；大马里奥受击缩小不扣血；小马里奥才真正扣血判死
    void applyDamage(int damage);

    bool isPlayer = true;
    bool is_big = false;
    bool transforming = false;
    Timer transform_timer;
};
