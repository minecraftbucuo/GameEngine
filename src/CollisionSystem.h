//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "GameObject.h"
#include <vector>
#include <memory>
#include "EventBus.h"
#include <iostream>
#include "Events.h"
#include "Collision.h"

class CollisionSystem {
public:
    void addObject(const std::shared_ptr<GameObject>& obj) {
        objects.push_back(obj);
    }
    void checkCollisions() const {
        for (size_t i = 0; i < objects.size(); i++) {
            for (size_t j = i + 1; j < objects.size(); j++) {
                const auto a = objects[i];
                const auto b = objects[j];

                const auto a_c = a->getComponent<Collision>();
                if (!a_c) continue;
                // float maxX = std::max(a->posX + a->width, b->posX + b->width);
                // float minX = std::min(a->posX, b->posX);
                // float maxY = std::max(a->posY + a->height, b->posY + b->height);
                // float minY = std::min(a->posY, b->posY);

                // std::cout << a->height + b->height << ' ' << maxY - minY << std::endl;

                if (auto b_c = b->getComponent<Collision>(); a_c->checkCollision(*b_c)) {
                    const sf::Vector2f a_speed = a->getSpeed();
                    const sf::Vector2f b_speed = b->getSpeed();
                    const sf::Vector2f a_position = a->getPosition();
                    const sf::Vector2f b_position = b->getPosition();
                    EventBus::getInstance().publish("onCollision" + a->getTag(),
                        CollisionEvent{ a, b, a_speed, b_speed, a_position, b_position });
                    EventBus::getInstance().publish("onCollision" + b->getTag(),
                        CollisionEvent{ b, a, b_speed, a_speed, b_position, a_position });
                }
            }
        }
    }

    [[nodiscard]] const std::vector<std::shared_ptr<GameObject>>* getObjects() const {
        return &objects;
    }

private:
    std::vector<std::shared_ptr<GameObject>> objects;
};


