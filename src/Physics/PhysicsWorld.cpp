//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsWorld.h"
#include "PhysicsContactListener.h"
#include "ConfigManager.h"
#include "PhysicsTypes.h"

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
}

PhysicsWorld::~PhysicsWorld() {
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

} // namespace physics
