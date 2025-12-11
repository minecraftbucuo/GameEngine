//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include "CollisionHandle.h"
#include "GameObject.h"
#include "BoxCollision.h"
#include "CircleCollision.h"

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
        auto& this_ = event.a;
        auto& other = event.b;

        if (!this_->moveAble) return;
        // this_->speed.y = -this_->speed.y;
        this_->speed.y = -this_->speed.y * 0.28f;

        if (std::abs(this_->speed.y) <= 2.f) {
            this_->speed.y = 0;
        }

        // 防止穿进地面
        this_->position.y = other->position.y - this_->size.y;
        this_->setPosition(this_->position.x, this_->position.y);
        this_->getComponent<Collision>()->update(sf::Time::Zero);
    }

    void handleCollisionWithCircle(const CollisionEvent &event) override {
        std::cout << "CircleCollisionHandle::handleCollisionWithCircle" << std::endl;
    }
};



