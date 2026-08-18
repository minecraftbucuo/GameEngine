//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsContactListener.h"
#include "GameObject.h"
#include "Scene.h"
#include "EventBus.h"
#include "Events.h"
#include "PhysicsTypes.h"

namespace physics {

GameObject* PhysicsContactListener::getGameObject(b2Contact* contact, const bool isA) {
    b2Fixture* fixture = isA ? contact->GetFixtureA() : contact->GetFixtureB();
    if (!fixture) return nullptr;
    return reinterpret_cast<GameObject*>(fixture->GetUserData().pointer);
}

void PhysicsContactListener::BeginContact(b2Contact* contact) {
    GameObject* a = getGameObject(contact, true);
    GameObject* b = getGameObject(contact, false);
    if (!a || !b) return;

    // 组装 CollisionEvent，通过 EventBus 发布到现有订阅者
    const sf::Vector2f a_pos = a->getPosition();
    const sf::Vector2f b_pos = b->getPosition();

    // 从 b2Body 取速度（如果有）
    b2Body* bodyA = contact->GetFixtureA()->GetBody();
    b2Body* bodyB = contact->GetFixtureB()->GetBody();
    const sf::Vector2f a_speed = toPixels(bodyA->GetLinearVelocity());
    const sf::Vector2f b_speed = toPixels(bodyB->GetLinearVelocity());

    // 从场景找回 shared_ptr
    Scene* scene = a->getScene();
    if (!scene) return;
    auto objA = scene->findGameObjectById(a->getId());
    auto objB = scene->findGameObjectById(b->getId());
    if (!objA || !objB) return;

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

} // namespace physics
