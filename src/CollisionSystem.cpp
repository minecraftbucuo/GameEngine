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

    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t j = i + 1; j < objects.size(); j++) {
            const auto a = objects[i];
            const auto b = objects[j];
            if (!a->getMoveAble() && !b->getMoveAble()) continue;
            if (!a->isActive() || !b->isActive()) continue;

            const auto a_c = a->getComponent<Collision>();
            if (!a_c || !a_c->getActive()) continue;
            // float maxX = std::max(a->posX + a->width, b->posX + b->width);
            // float minX = std::min(a->posX, b->posX);
            // float maxY = std::max(a->posY + a->height, b->posY + b->height);
            // float minY = std::min(a->posY, b->posY);

            // std::cout << a->height + b->height << ' ' << maxY - minY << std::endl;

            if (auto b_c = b->getComponent<Collision>(); b_c->getActive() && a_c->checkCollision(*b_c)) {
                const sf::Vector2f a_speed = a->getSpeed();
                const sf::Vector2f b_speed = b->getSpeed();
                const sf::Vector2f ac_position = a_c->getCollisionPosition();
                const sf::Vector2f bc_position = b_c->getCollisionPosition();
                EventBus::getInstance().publish("onCollision" + a->getTag(),
                    CollisionEvent{ a, b, a_speed, b_speed, ac_position, bc_position });
                EventBus::getInstance().publish("onCollision" + b->getTag(),
                    CollisionEvent{ b, a, b_speed, a_speed, bc_position, ac_position });
            }
        }
    }
}

std::vector<std::shared_ptr<GameObject>>* CollisionSystem::getObjects() {
    return &objects;
}
