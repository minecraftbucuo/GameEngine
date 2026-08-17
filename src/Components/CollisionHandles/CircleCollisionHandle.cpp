//
// Created by MINEC on 2026/5/8.
//

#include "CircleCollisionHandle.h"

#include "BoxCollision.h"
#include <cmath>
#include "GameObject.h"
#include "MoveComponent.h"

CircleCollisionHandle::CircleCollisionHandle() {
    collisionHandlers[typeid(BoxCollision).hash_code()] = [this](const CollisionEvent& event) {
        this->handleCollisionWithBox(event);
    };
    collisionHandlers[typeid(CircleCollision).hash_code()] = [this](const CollisionEvent& event) {
        this->handleCollisionWithCircle(event);
    };
}

void CircleCollisionHandle::handleCollisionWithBox(const CollisionEvent& event) {
    handle(event);
}

void CircleCollisionHandle::handleCollisionWithCircle(const CollisionEvent& event) {
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

    // 冲量：沿法向把本方推离对方（双方各自的事件都会执行，碰撞是相互的）
    moveComponent->addSpeed(dir * relativeSpeed_len * 0.28f);

    // 位置分离：双方各分摊一半修正距离，避免双方各自移动完整重叠距离导致总分离 2 倍。
    // 若对方不可移动（如地面），则由本方推完整距离（对方不动）。
    const float move_dis = (this_->getSize().x / 2 + other->getSize().x / 2 - dir_len);
    if (other->getMoveAble()) {
        moveComponent->addPosition(move_dis * 0.5f * dir);
    } else {
        moveComponent->addPosition(move_dis * dir);
    }
}
