//
// Created by MINEC on 2025/12/10.
//

#include "CircleCollision.h"
#include "BoxCollision.h"
#include "GameObject.h"
#include <cmath>

#include "ConfigManager.h"
#include "Core/Types.h"

CircleCollision::CircleCollision(const float x, const float y, const float radius) {
    this->position = eng::Vec2f(x, y);
    this->radius = radius;
}

void CircleCollision::update(const eng::Time& deltaTime) {
    // this->posX = owner->posX + owner->width / 2;
    // this->posY = owner->posY + owner->height / 2;
    // this->position = owner->getPosition() + owner->getSize() * 0.5f;
}
#ifndef SERVER_BUILD
void CircleCollision::render(sf::RenderWindow *window) {
    if (!CONFIG.game.debug) return;
    sf::CircleShape shape(radius);
    shape.setPosition(this->getPos() - eng::Vec2f(radius, radius));
    // std::cout << shape.getPosition().x << " " << shape.getPosition().y << std::endl;
    shape.setFillColor(eng::Color::Transparent);
    shape.setOutlineColor(eng::Color::Red);
    shape.setOutlineThickness(2);
    window->draw(shape);
}
#endif

void CircleCollision::setPosition(const eng::Vec2f &position) {
    this->position = owner->getPosition() + owner->getSize() * 0.5f;
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
eng::Vec2f CircleCollision::getPos() const {
    return this->position + this->offset;
}

float CircleCollision::getPosX() const {
    return this->position.x + this->offset.x;
}

float CircleCollision::getPosY() const {
    return this->position.y + this->offset.y;
}

eng::Vec2f CircleCollision::getCollisionPosition() const {
    return this->position + this->offset - eng::Vec2f(radius, radius);
}

