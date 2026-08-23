//
// Created by MINEC on 2026/5/8.
//
#include "Core/Types.h"
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
    this->position = eng::Vec2f(x, y);
    this->size = eng::Vec2f(radius * 2, radius * 2);

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
        [this](const CollisionEvent& collisionEvent) {
            if (const auto& handler = this->getComponent<CollisionHandle>()) {
                handler->handleCollision(collisionEvent);
            }
        }
    );
}
#endif