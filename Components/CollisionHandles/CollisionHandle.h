//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include <functional>
#include <unordered_map>
#include "Component.h"
#include "Events.h"
#include <iostream>
#include "Collision.h"
#include "GameObject.h"
#include "MoveComponent.h"

class CollisionHandle : public Component {
public:
    void start() override {}
    void update(sf::Time deltaTime) override {}
    void render(sf::RenderWindow* window) override {}
    void handleEvent(sf::Event& event) override {}

    void handleCollision(const CollisionEvent& event) {
        size_t hash_code = typeid(*event.b->getComponent<Collision>()).hash_code();
        if (this->collisionHandlers.find(hash_code) == this->collisionHandlers.end()) {
            std::cout << "error: no match collision handler for " << typeid(*event.b->getComponent<Collision>()).name() << std::endl;
            return;
        }
        this->collisionHandlers[hash_code](event);
    }

    virtual void handleCollisionWithBox(const CollisionEvent& event) = 0;
    virtual void handleCollisionWithCircle(const CollisionEvent& event) = 0;

    static void handle(const CollisionEvent& event) {
        auto& this_ = event.a;
        auto& other = event.b;

        // std::cout << this_->getTag() << ' ' << other->getTag() << std::endl;

        if (!this_->getMoveAble()) return;
        std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
        if (!moveComponent) return;

        // 计算 x 方向和 y 方向的重合度
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
            float right_x = std::abs(event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
            float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
            if (right_x < left_x) {
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
            float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
            float bottom_y = std::abs(event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
            if (top_y > bottom_y) {
                moveComponent->setPositionY(event.b_position.y - this_->getSize().y);
            } else {
                moveComponent->setPositionY(event.b_position.y + other->getSize().y);
            }
        }
    }

protected:
    std::unordered_map<size_t, std::function<void(CollisionEvent)>> collisionHandlers;
};
