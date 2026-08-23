//
// Created by MINEC on 2026/5/8.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Circle.h"
#include "CameraComponent.h"
#include "EventBus.h"
#include "Events.h"
#include "GravityComponent.h"
#include "CollisionHandle.h"
#include "CircleCollisionHandle.h"
#include "MarioJumpState.h"
#include "CollisionSystem.h"
#include "Scene.h"

Circle::Circle(const float x, const float y, const float radius, const std::string& tag) {
    this->position = eng::Vec2f(x, y);
    this->size = eng::Vec2f(radius * 2, radius * 2);

    this->addComponent<Collision, CircleCollision>(this->position.x + radius, this->position.y + radius, this->size.x / 2);
    this->addComponent<CollisionHandle, CircleCollisionHandle>();
    this->addComponent<GravityComponent>();
    this->addComponent<MoveComponent>();

    this->tag = tag + ":" + std::to_string(this->id);

    className = "Circle";
}

Circle::~Circle() {
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

// SDL3 迁移 6e：sf::CircleShape 数据化，与 Player 同构（半径 = size.x/2）
void Circle::render(eng::Renderer& renderer) {
    renderer.drawCircle(position + size * 0.5f, size.x * 0.5f, eng::Color::White);
    renderComponents(renderer);
}

void Circle::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            if (const auto& handler = this->getComponent<CollisionHandle>()) {
                handler->handleCollision(collisionEvent);
                this->getComponent<GravityComponent>()->setActive(false);
            }
        }
    );
}

void Circle::update(eng::Time deltaTime) {
    if (needGravity()) {
        this->getComponent<GravityComponent>()->setActive(true);
    }
    GameObject::update(deltaTime);
}

bool Circle::needGravity() {
    auto collision = this->getComponent<Collision>();
    eng::Vec2f dy = eng::Vec2f(0.f, 1.f);
    collision->setCollisionPosition(collision->getCollisionPosition() + dy);

    const auto& game_objects = *getScene()->getCollisionSystem()->getObjects();

    for (auto& game_object : game_objects) {
        if (game_object->getTag() == this->getTag()) continue;
        auto other_collision = game_object->getComponent<Collision>();
        if (!other_collision) continue;
        if (other_collision->checkCollision(*collision)) {
            collision->setCollisionPosition(collision->getCollisionPosition() - dy);
            return false;
        }
    }
    collision->setCollisionPosition(collision->getCollisionPosition() - dy);
    return true;
}

void Circle::setPosition(const float x, const float y) {
    GameObject::setPosition(x, y);
}

void Circle::setSpeed(const float x, const float y) {
    this->speed.x = x;
    this->speed.y = y;
}
#endif
