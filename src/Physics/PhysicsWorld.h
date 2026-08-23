//
// Created by MINEC on 2026/8/18.
//

#pragma once

#include <box2d/box2d.h>
#include <SFML/System/Time.hpp>
#include <memory>
#include "Core/Types.h"

namespace eng { class Renderer; }

namespace physics {

class PhysicsContactListener;
class PhysicsDebugDraw;

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // 固定步累加器：每帧累加 dt，每 fixedStep 秒调用一次 b2World::Step
    void step(const eng::Time& frameTime);

    // 创建/销毁 body
    b2Body* createBody(const b2BodyDef* def);
    void destroyBody(b2Body* body);

    // 清空所有 body（场景退出时调用）
    void clear();

    // 获取原始 b2World（PhysicsBodyComponent 等需要）
    b2World* getWorld();
    const b2World* getWorld() const;

#ifndef SERVER_BUILD
    // 调试绘制：绘制所有 body 形状/质心/接触点（CONFIG.game.debug 时由 PhysicsTestScene::render 调用）
    void renderDebug(eng::Renderer* renderer);
#endif

private:
    b2World* world{};
    PhysicsContactListener* contactListener{};
#ifndef SERVER_BUILD
    PhysicsDebugDraw* debugDraw{};
#endif
    float accumulator = 0.0f;
    float fixedStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    int positionIterations = 3;
};

} // namespace physics
