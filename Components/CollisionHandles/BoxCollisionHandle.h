//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include "CollisionHandle.h"
#include "GameObject.h"
#include "BoxCollision.h"
#include "CircleCollision.h"
#include "MoveComponent.h"

class BoxCollisionHandle : public CollisionHandle {
public:
    BoxCollisionHandle() {
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

        if (!this_->getMoveAble()) return;
        std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
        if (!moveComponent) return;

        const float dx = std::min(event.a_position.x + this_->getSize().x,
            event.b_position.x + other->getSize().x) - std::max(event.a_position.x, event.b_position.x);
        const float dy = std::min(event.a_position.y + this_->getSize().y,
            event.b_position.y + other->getSize().y) - std::max(event.a_position.y, event.b_position.y);

        if (std::abs(dx - dy) <= 0.1f) return;
        // 水平碰撞
        if (dx < dy) {
            const float relativeSpeedX = event.b_speed.x - event.a_speed.x;
            moveComponent->setSpeedX(relativeSpeedX * 0.28f);
            if (std::abs(this_->getSpeed().x) <= 2.f) {
                moveComponent->setSpeedX(0.f);
            }
            if (event.a_position.x < event.b_position.x) {
                moveComponent->setPositionX(event.b_position.x - this_->getSize().x);
            } else {
                moveComponent->setPositionX(event.b_position.x + other->getSize().x);
            }
        } else {
            const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
            moveComponent->setSpeedY(relativeSpeedY * 0.28f);
            if (std::abs(this_->getSpeed().y) <= 2.f) {
                moveComponent->setSpeedY(0.f);
            }
            if (event.a_position.y < event.b_position.y) {
                moveComponent->setPositionY(event.b_position.y - this_->getSize().y);
            } else {
                moveComponent->setPositionY(event.b_position.y + other->getSize().y);
            }
        }
    }

    void handleCollisionWithCircle(const CollisionEvent &event) override {
        std::cout << "BoxCollisionHandle::handleCollisionWithCircle" << std::endl;
    }
};
