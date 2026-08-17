//
// Created by MINEC on 2025/12/10.
//

#include "CircleCollision.h"
#include "BoxCollision.h"
#include "GameObject.h"
#include <cmath>

#include "ConfigManager.h"

CircleCollision::CircleCollision(const float x, const float y, const float radius) {
    this->position = sf::Vector2f(x, y);
    this->radius = radius;
}

void CircleCollision::update(const sf::Time& deltaTime) {
    // this->posX = owner->posX + owner->width / 2;
    // this->posY = owner->posY + owner->height / 2;
    // this->position = owner->getPosition() + owner->getSize() * 0.5f;
}
#ifndef SERVER_BUILD
void CircleCollision::render(sf::RenderWindow *window) {
    if (!CONFIG.game.debug) return;
    sf::CircleShape shape(radius);
    shape.setPosition(this->getPos() - sf::Vector2f(radius, radius));
    // std::cout << shape.getPosition().x << " " << shape.getPosition().y << std::endl;
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color::Red);
    shape.setOutlineThickness(2);
    window->draw(shape);
}
#endif

// C4：真正使用传入参数（不再忽略并硬取 owner 位置）。
// 传入的是碰撞体左上角（与基类 Box 语义一致，如 MoveComponent 同步 GameObject 位置时），
// 内部按圆心存储：position = 左上角 - offset + (r, r)，保证 getCollisionPosition() = 左上角。
void CircleCollision::setPosition(const sf::Vector2f &_position) {
    this->position = _position - this->offset + sf::Vector2f(radius, radius);
}

bool CircleCollision::checkCollision(const Collision &other) const {
    return other.checkCollisionWithCircle(*this);
}

bool CircleCollision::checkCollisionWithBox(const BoxCollision &other) const {
    return other.checkCollisionWithCircle(*this);
}


bool CircleCollision::checkCollisionWithCircle(const CircleCollision &other) const {
    const float distance = sqrtf(powf(other.getPos().x - getPosX(), 2) + powf(other.getPos().y - getPosY(), 2));
    return distance < radius + other.getRadius();
}

float CircleCollision::getRadius() const {
    return radius;
}

// 返回碰撞圆的圆心坐标
sf::Vector2f CircleCollision::getPos() const {
    return this->position + this->offset;
}

// B3：圆心即中心
sf::Vector2f CircleCollision::getCenter() const {
    return this->position + this->offset;
}

float CircleCollision::getPosX() const {
    return this->position.x + this->offset.x;
}

float CircleCollision::getPosY() const {
    return this->position.y + this->offset.y;
}

sf::Vector2f CircleCollision::getCollisionPosition() const {
    return this->position + this->offset - sf::Vector2f(radius, radius);
}

