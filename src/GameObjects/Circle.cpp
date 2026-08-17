//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD
#include "Circle.h"
#include "CameraComponent.h"
#include "EventBus.h"
#include "Events.h"
#include "GravityComponent.h"
#include "MarioJumpState.h"
#include "CollisionSystem.h"
#include "Scene.h"

Circle::Circle(const float x, const float y, const float radius, const std::string& tag) {
    shape.setRadius(radius);
    this->position = sf::Vector2f(x, y);
    this->size = sf::Vector2f(radius * 2, radius * 2);
    shape.setPosition(x, y);

    // 物理响应由 CollisionSystem 求解器统一处理，不再添加 CollisionHandle 组件（B1/B6 清理）
    this->addComponent<Collision, CircleCollision>(this->position.x + radius, this->position.y + radius, this->size.x / 2);
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
        [this](const CollisionEvent&) {
            // B1：物理响应（位置修正/速度冲量）已由 CollisionSystem 的迭代求解器统一处理，
            // 事件仅保留给游戏逻辑；Circle 无游戏逻辑，这里保留空订阅以免 EventBus 告警。
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
