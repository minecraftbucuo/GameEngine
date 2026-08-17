//
// Created by MINEC on 2026/5/8.
//

#include "CollisionHandle.h"
#include "Collision.h"
#include "GameObject.h"
#include "MoveComponent.h"
#include <cmath>

#include "Logger.h"

class Collision;

void CollisionHandle::handleCollision(const CollisionEvent& event) {
    size_t hash_code = typeid(*event.b->getComponent<Collision>()).hash_code();
    if (this->collisionHandlers.find(hash_code) == this->collisionHandlers.end()) {
        LOG_ERROR_FMT("error: no match collision handler for {}", typeid(*event.b->getComponent<Collision>()).name());
        return;
    }
    this->collisionHandlers[hash_code](event);
}

void CollisionHandle::handle(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    if (!this_->getMoveAble()) return;
    std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
    if (!moveComponent) return;

    // 位置分离辅助：双方各分摊一半修正距离（对方不可移动时由本方推完整距离），
    // 避免双方各自移动完整重叠距离导致总分离 2 倍
    auto moveToHalfX = [&](const float targetX) {
        const auto collision = this_->getComponent<Collision>();
        if (!collision) return;
        const sf::Vector2f cur = collision->getCollisionPosition();
        const float x = other->getMoveAble() ? cur.x + (targetX - cur.x) * 0.5f : targetX;
        moveComponent->moveCollisionTo(sf::Vector2f(x, cur.y));
    };
    auto moveToHalfY = [&](const float targetY) {
        const auto collision = this_->getComponent<Collision>();
        if (!collision) return;
        const sf::Vector2f cur = collision->getCollisionPosition();
        const float y = other->getMoveAble() ? cur.y + (targetY - cur.y) * 0.5f : targetY;
        moveComponent->moveCollisionTo(sf::Vector2f(cur.x, y));
    };

    // 计算 x 方向和 y 方向的重合度
    const float dx = std::min(event.a_position.x + this_->getSize().x,
                              event.b_position.x + other->getSize().x) - std::max(
        event.a_position.x, event.b_position.x);
    const float dy = std::min(event.a_position.y + this_->getSize().y,
                              event.b_position.y + other->getSize().y) - std::max(
        event.a_position.y, event.b_position.y);

    // 选择重叠更小的方向进行解析，避免角落穿透
    if (dx < dy) {
        const float relativeSpeedX = event.b_speed.x - event.a_speed.x;
        moveComponent->setSpeedX(relativeSpeedX * 0.28f);
        if (std::abs(this_->getSpeed().x) <= 2.f) {
            moveComponent->setSpeedX(0.f);
        }
        float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveToHalfX(event.b_position.x - this_->getSize().x);
        }
        else {
            moveToHalfX(event.b_position.x + other->getSize().x);
        }
    }
    else {
        const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
        moveComponent->setSpeedY(relativeSpeedY * 0.28f);
        if (std::abs(this_->getSpeed().y) <= 2.f) {
            moveComponent->setSpeedY(0.f);
        }
        float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
        float bottom_y = std::abs(
            event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            moveToHalfY(event.b_position.y - this_->getSize().y);
            if (std::abs(this_->getSpeed().y) <= 150.f) {
                moveComponent->setSpeedY(0.f);
            }
        }
        else {
            moveToHalfY(event.b_position.y + other->getSize().y);
        }
    }
}