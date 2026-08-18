//
// Created by MINEC on 2026/8/18.
//

#pragma once

#include <box2d/box2d.h>
#include <SFML/System/Vector2.hpp>

class GameObject;

namespace physics {

// 桥接 Box2D 接触事件到引擎 EventBus
// BeginContact → publish("onCollision"+tag, CollisionEvent)
// EndContact   → publish("onCollisionEnd"+tag)（新事件，旧代码不订阅则无影响）
class PhysicsContactListener : public b2ContactListener {
public:
    void BeginContact(b2Contact* contact) override;
    void EndContact(b2Contact* contact) override;

private:
    // 从 fixture userData 取 GameObject*
    static GameObject* getGameObject(b2Contact* contact, bool isA);
};

} // namespace physics
