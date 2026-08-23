//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsContactListener.h"
#include "GameObject.h"
#include "Scene.h"
#include "EventBus.h"
#include "Events.h"
#include "PhysicsTypes.h"
#include "Logger.h"
#include "Core/Types.h"

namespace physics {

GameObject* PhysicsContactListener::getGameObject(b2Contact* contact, const bool isA) {
    b2Fixture* fixture = isA ? contact->GetFixtureA() : contact->GetFixtureB();
    if (!fixture) return nullptr;
    return reinterpret_cast<GameObject*>(fixture->GetUserData().pointer);
}

void PhysicsContactListener::BeginContact(b2Contact* contact) {
    GameObject* a = getGameObject(contact, true);
    GameObject* b = getGameObject(contact, false);
    if (!a || !b) {
        LOG_WARN("PhysicsContactListener::BeginContact - a or b is null");
        return;
    }

    // 组装 CollisionEvent，通过 EventBus 发布到现有订阅者
    const eng::Vec2f a_pos = a->getPosition();
    const eng::Vec2f b_pos = b->getPosition();

    // 从 b2Body 取速度（如果有）
    b2Body* bodyA = contact->GetFixtureA()->GetBody();
    b2Body* bodyB = contact->GetFixtureB()->GetBody();
    const eng::Vec2f a_speed = toPixels(bodyA->GetLinearVelocity());
    const eng::Vec2f b_speed = toPixels(bodyB->GetLinearVelocity());

    // 从场景找回 shared_ptr
    Scene* scene = a->getScene();
    if (!scene) {
        LOG_WARN("PhysicsContactListener::BeginContact - scene is null, a.tag=" + a->getTag());
        return;
    }
    auto objA = scene->findGameObjectById(a->getId());
    auto objB = scene->findGameObjectById(b->getId());
    if (!objA || !objB) {
        LOG_WARN("PhysicsContactListener::BeginContact - findGameObjectById failed, a.id=" + std::to_string(a->getId())
                 + " b.id=" + std::to_string(b->getId()));
        return;
    }

    CollisionEvent event{ objA, objB, a_speed, b_speed, a_pos, b_pos };
    EventBus::getInstance().publish("onCollision" + a->getTag(), event);

    // 反向也发一份（与原 CollisionSystem 行为一致）
    CollisionEvent eventRev{ objB, objA, b_speed, a_speed, b_pos, a_pos };
    EventBus::getInstance().publish("onCollision" + b->getTag(), eventRev);
}

void PhysicsContactListener::EndContact(b2Contact* contact) {
    GameObject* a = getGameObject(contact, true);
    GameObject* b = getGameObject(contact, false);
    if (!a || !b) return;

    // 发布结束事件（新事件名，不影响现有订阅者）
    CollisionEvent event{ nullptr, nullptr, {}, {}, a->getPosition(), b->getPosition() };
    EventBus::getInstance().publish("onCollisionEnd" + a->getTag(), event);
    EventBus::getInstance().publish("onCollisionEnd" + b->getTag(), event);
}

void PhysicsContactListener::PreSolve(b2Contact* contact, const b2Manifold* /*oldManifold*/) {
    // 默认不干预。具体对象（如单向砖块）迁移时可在 owner 上检查标签，
    // 用 contact->SetEnabled(false) 禁用本次接触。
    // 示例：若 a 是玩家、b 是砖块，且法线向上（玩家从下方撞），则 SetEnabled(false) 让玩家穿过。
    (void)contact;
}

} // namespace physics
