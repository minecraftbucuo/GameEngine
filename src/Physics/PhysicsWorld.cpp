//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsWorld.h"
#include "PhysicsContactListener.h"
#include "ConfigManager.h"
#include "PhysicsTypes.h"
#ifndef SERVER_BUILD
#include "PhysicsDebugDraw.h"
#include <SFML/Graphics/RenderWindow.hpp>
#endif

namespace physics {

PhysicsWorld::PhysicsWorld() {
    // 从配置读取参数
    fixedStep = CONFIG.game.physicsFixedStep;
    velocityIterations = CONFIG.game.physicsVelocityIterations;
    positionIterations = CONFIG.game.physicsPositionIterations;

    // 重力向量：Y 向下（SFML 坐标系），像素/s² → m/s²
    const float gravityMps = physics::toMeters(CONFIG.game.gravity);
    world = new b2World(b2Vec2(0.0f, gravityMps));

    // 接触监听器，桥接到 EventBus
    contactListener = new PhysicsContactListener();
    world->SetContactListener(contactListener);

#ifndef SERVER_BUILD
    // 调试绘制（形状 + 质心变换轴；不含 e_pairBit 的宽相位配对连线）
    debugDraw = new PhysicsDebugDraw();
    debugDraw->SetFlags(b2Draw::e_shapeBit | b2Draw::e_centerOfMassBit);
    world->SetDebugDraw(debugDraw);
#endif
}

PhysicsWorld::~PhysicsWorld() {
#ifndef SERVER_BUILD
    if (world) world->SetDebugDraw(nullptr);
    delete debugDraw;
    debugDraw = nullptr;
#endif
    delete contactListener;
    contactListener = nullptr;
    delete world;
    world = nullptr;
}

void PhysicsWorld::step(const sf::Time& frameTime) {
    if (!world) return;

    // 累加器实现固定步，防止螺旋死亡（每帧最多 5 步）
    accumulator += frameTime.asSeconds();
    int steps = 0;
    while (accumulator >= fixedStep && steps < 5) {
        world->Step(fixedStep, velocityIterations, positionIterations);
        accumulator -= fixedStep;
        ++steps;
    }
    // 超出上限时丢弃剩余时间，避免累积爆炸
    if (steps >= 5) {
        accumulator = 0.0f;
    }
}

b2Body* PhysicsWorld::createBody(const b2BodyDef* def) {
    return world->CreateBody(def);
}

void PhysicsWorld::destroyBody(b2Body* body) {
    if (world && body) {
        world->DestroyBody(body);
    }
}

void PhysicsWorld::clear() {
    if (!world) return;
    // 销毁所有 body
    b2Body* body = world->GetBodyList();
    while (body) {
        b2Body* next = body->GetNext();
        world->DestroyBody(body);
        body = next;
    }
    accumulator = 0.0f;
}

b2World* PhysicsWorld::getWorld() {
    return world;
}

const b2World* PhysicsWorld::getWorld() const {
    return world;
}

#ifndef SERVER_BUILD
void PhysicsWorld::renderDebug(sf::RenderWindow* window) {
    if (!world || !debugDraw || !window) return;
    debugDraw->setWindow(window);
    world->DebugDraw();
    debugDraw->drawVelocities(world); // 速度方向箭头
}
#endif

} // namespace physics
