//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD
#include "Player.h"

#include "CircleCollisionHandle.h"
#include "CollisionHandle.h"
#include "GravityComponent.h"
#include "MoveComponent.h"
#include "Events.h"
#include "GameObject.h"
#include "Collision.h"
#include "CircleCollision.h"

Player::Player(const float x, const float y, const float radius, const std::string& tag) {
    shape.setRadius(radius);
    this->position = sf::Vector2f(x, y);
    this->size = sf::Vector2f(radius * 2, radius * 2);
    shape.setPosition(x, y);

    this->addComponent<Collision, CircleCollision>(this->position.x + radius, this->position.y + radius, this->size.x / 2);
    this->addComponent<CollisionHandle, CircleCollisionHandle>();
    // this->addComponent<Collision, BoxCollision, true>();
    // this->addComponent<CollisionHandle, BoxCollisionHandle>();

    this->addComponent<MoveComponent>();
    this->addComponent<GravityComponent>();
    this->tag = tag + ":" + std::to_string(this->id);
    className = "Player";
}

void Player::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent&) {
            // B1：物理响应已由 CollisionSystem 的迭代求解器统一处理，事件仅保留给游戏逻辑。
        }
    );
}
#endif