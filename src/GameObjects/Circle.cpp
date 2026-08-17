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
#include "CollisionSystem.h"
#include "Scene.h"

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
                // 注意：不再在这里 setActive(false) 关闭重力（A5）。
                // 之前"碰到任何东西就关重力"与 needGravity() 探针、GravityComponent 的
                // 底部判定三套逻辑互相打架，是抖动源之一。现在静止由碰撞处理的
                // 法向速度清零（A2）保证，重力保持开启也不会积累速度。
            }
        }
    );
}

void Circle::update(sf::Time deltaTime) {
    // 重力开关统一由 needGravity() 探针决定：下方悬空则开启重力，下方有支撑则关闭。
    // 静止堆叠中的球重力关闭、不累加速度（A5 删除了事件里的关闭，必须在这里补上关闭路径，
    // 否则重力一旦开启就永远施加，静止的球每帧都在被重力累加速度）。
    const auto gravity = this->getComponent<GravityComponent>();
    if (needGravity()) {
        gravity->setActive(true);
    } else {
        gravity->setActive(false);
    }
    GameObject::update(deltaTime);
}

bool Circle::needGravity() {
    auto collision = this->getComponent<Collision>();
    sf::Vector2f dy = sf::Vector2f(0.f, 1.f);
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
    shape.setPosition(x, y);
}

void Circle::setSpeed(const float x, const float y) {
    this->speed.x = x;
    this->speed.y = y;
}
#endif
