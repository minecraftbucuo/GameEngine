//
// Created by MINEC on 2026/6/2.
//

#include "CollisionSystem.h"

#include "Collision.h"
#include "GameObject.h"
#include "EventBus.h"
#include "Events.h"
#include "Core/Types.h"

void CollisionSystem::addObject(const std::shared_ptr<GameObject>& obj) {
    objects.push_back(obj);
}

void CollisionSystem::checkCollisions() {
    std::erase_if(objects, [](const auto& obj) { return obj->isDestroy(); });

    struct Pending { std::string tag; CollisionEvent event; };
    std::vector<Pending> pending;

    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t j = i + 1; j < objects.size(); j++) {
            const auto a = objects[i];
            const auto b = objects[j];
            if (!a->getMoveAble() && !b->getMoveAble()) continue;
            if (!a->isActive() || !b->isActive()) continue;
            const auto a_c = a->getComponent<Collision>();
            if (!a_c || !a_c->getActive()) continue;
            if (auto b_c = b->getComponent<Collision>();
                b_c->getActive() && a_c->checkCollision(*b_c)) {
                const eng::Vec2f a_speed = a->getSpeed();
                const eng::Vec2f b_speed = b->getSpeed();
                const eng::Vec2f ac_position = a_c->getCollisionPosition();
                const eng::Vec2f bc_position = b_c->getCollisionPosition();
                pending.push_back({"onCollision" + a->getTag(),
                    CollisionEvent{a, b, a_speed, b_speed, ac_position, bc_position}});
                pending.push_back({"onCollision" + b->getTag(),
                    CollisionEvent{b, a, b_speed, a_speed, bc_position, ac_position}});
            }
        }
    }
    for (const auto& p : pending) {
        EventBus::getInstance().publish(p.tag, p.event);
    }
}

std::vector<std::shared_ptr<GameObject>>* CollisionSystem::getObjects() {
    return &objects;
}
