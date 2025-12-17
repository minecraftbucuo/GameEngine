//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include <cmath>

#include "CollisionHandle.h"
#include "GameObject.h"
#include "BoxCollision.h"
#include "CircleCollision.h"
#include "MoveComponent.h"

class CircleCollisionHandle : public CollisionHandle {
public:
    CircleCollisionHandle() {
        collisionHandlers[typeid(BoxCollision).hash_code()] = [this](const CollisionEvent& event) {
            this->handleCollisionWithBox(event);
        };
        collisionHandlers[typeid(CircleCollision).hash_code()] = [this](const CollisionEvent& event) {
            this->handleCollisionWithCircle(event);
        };
    }

    void handleCollisionWithBox(const CollisionEvent &event) override {
        handle(event);
    }

    void handleCollisionWithCircle(const CollisionEvent &event) override {
        auto &this_ = event.a;
        auto &other = event.b;

        if (!this_->getMoveAble()) return;
        const std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
        if (!moveComponent) return;

        const sf::Vector2f center_a = event.a_position + this_->getSize() * 0.5f;
        const sf::Vector2f center_b = event.b_position + other->getSize() * 0.5f;

        sf::Vector2f dir = center_a - center_b;
        const float dir_len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        const sf::Vector2f relativeSpeed = event.a_speed - event.b_speed;
        const float relativeSpeed_len = std::sqrt(relativeSpeed.x * relativeSpeed.x + relativeSpeed.y * relativeSpeed.y);

        dir /= dir_len;

        moveComponent->addSpeed(dir * relativeSpeed_len * 0.28f);

        // 完全分离两个圆形
        const float move_dis = (this_->getSize().x / 2 + other->getSize().x / 2 - dir_len);
        moveComponent->addPosition(move_dis * dir);
    }
};



