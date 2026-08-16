//
// Created by MINEC on 2026/6/2.
//

#include "CollisionSystem.h"

#include "Collision.h"
#include "GameObject.h"
#include "EventBus.h"
#include "Events.h"

void CollisionSystem::addObject(const std::shared_ptr<GameObject>& obj) {
    objects.push_back(obj);
}

void CollisionSystem::checkCollisions() {

    // 清理已经销毁的游戏对象
    std::erase_if(objects, [](const auto& obj) {
        return obj->isDestroy();
    });

    // 第一阶段：收集所有碰撞对（基于同一帧的状态快照）
    struct CollisionPair {
        std::shared_ptr<GameObject> a;
        std::shared_ptr<GameObject> b;
        sf::Vector2f a_speed;
        sf::Vector2f b_speed;
        sf::Vector2f a_position;
        sf::Vector2f b_position;
    };
    std::vector<CollisionPair> collisionPairs;

    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t j = i + 1; j < objects.size(); j++) {
            const auto a = objects[i];
            const auto b = objects[j];
            if (!a->getMoveAble() && !b->getMoveAble()) continue;
            if (!a->isActive() || !b->isActive()) continue;

            const auto a_c = a->getComponent<Collision>();
            if (!a_c || !a_c->getActive()) continue;

            if (auto b_c = b->getComponent<Collision>(); b_c->getActive() && a_c->checkCollision(*b_c)) {
                collisionPairs.push_back({
                    a, b,
                    a->getSpeed(), b->getSpeed(),
                    a_c->getCollisionPosition(), b_c->getCollisionPosition()
                });
            }
        }
    }

    // 第二阶段：统一发布所有碰撞事件
    for (const auto& pair : collisionPairs) {
        EventBus::getInstance().publish("onCollision" + pair.a->getTag(),
            CollisionEvent{ pair.a, pair.b, pair.a_speed, pair.b_speed, pair.a_position, pair.b_position });
        EventBus::getInstance().publish("onCollision" + pair.b->getTag(),
            CollisionEvent{ pair.b, pair.a, pair.b_speed, pair.a_speed, pair.b_position, pair.a_position });
    }
}

std::vector<std::shared_ptr<GameObject>>* CollisionSystem::getObjects() {
    return &objects;
}