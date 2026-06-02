//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD
#include "Circle.h"
#include "CameraComponent.h"
#include "EventBus.h"
#include "Events.h"
#include "GravityComponent.h"
#include "CollisionHandle.h"
#include "CircleCollisionHandle.h"
#include "MarioJumpState.h"
#include "SceneContext.h"
#include "SceneManager.h"
#include "CollisionSystem.h"

Circle::Circle(const float x, const float y, const float radius, const std::string& tag) {
    shape.setRadius(radius);
    this->position = sf::Vector2f(x, y);
    this->size = sf::Vector2f(radius * 2, radius * 2);
    shape.setPosition(x, y);

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

void Circle::render(sf::RenderWindow* window) {
    window->draw(shape);
    renderComponents(window);
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

void Circle::update(sf::Time deltaTime) {
    if (needGravity()) {
        this->getComponent<GravityComponent>()->setActive(true);
    }
    GameObject::update(deltaTime);
}

bool Circle::needGravity() {
    auto collision = this->getComponent<Collision>();
    sf::Vector2f dy = sf::Vector2f(0.f, 1.f);
    collision->setCollisionPosition(collision->getCollisionPosition() + dy);

    const auto game_objects = *SceneContext::getInstance().
    getSceneManager()->getCurrentScene()->getCollisionSystem()->getObjects();

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
    shape.setPosition(x, y);
}

void Circle::setSpeed(const float x, const float y) {
    this->speed.x = x;
    this->speed.y = y;
}
#endif
