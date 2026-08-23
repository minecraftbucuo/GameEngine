//
// Created by MINEC on 2025/12/10.
//

#include "BoxCollision.h"
#include "CircleCollision.h"

#include "ConfigManager.h"
#include "GameObject.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"
#endif

BoxCollision::BoxCollision(const float x, const float y, const float width, const float height) {
    this->position.x = x;
    this->position.y = y;
    this->size.x = width;
    this->size.y = height;
}

BoxCollision::BoxCollision() = default;

void BoxCollision::start() {
    // std::cout << "BoxCollision::start()" << std::endl;
    this->position.x = owner->getPosition().x;
    this->position.y = owner->getPosition().y;
    this->size.x = owner->getSize().x;
    this->size.y = owner->getSize().y;
}

void BoxCollision::update(const eng::Time& deltaTime) {
    // this->posX = owner->posX;
    // this->posY = owner->posY;
    // this->position = owner->getPosition();
}
#ifndef SERVER_BUILD
void BoxCollision::render(eng::Renderer &renderer) {
    if (!CONFIG.game.debug) return;
    renderer.drawRect(eng::FloatRect(getCollisionPosition(), size),
                      eng::Color::Transparent, false, 2.f, eng::Color::Red);
}
#endif
bool BoxCollision::checkCollision(const Collision &other) const {
    return other.checkCollisionWithBox(*this);
}

bool BoxCollision::checkCollisionWithBox(const BoxCollision &other) const {
    const float maxX = std::max(getPosX() + size.x, other.getPosX() + other.getWidth());
    const float minX = std::min(getPosX(), other.getPosX());
    const float maxY = std::max(getPosY() + size.y, other.getPosY() + other.getHeight());
    const float minY = std::min(getPosY(), other.getPosY());
    return ((maxX - minX < size.x + other.getWidth()) && (maxY - minY < size.y + other.getHeight()));
}

bool BoxCollision::checkCollisionWithCircle(const CircleCollision &other) const {
    // 找到矩形上离圆心最近的点
    const float closestX = std::max(getPosX(), std::min(other.getPosX(), getPosX() + size.x));
    const float closestY = std::max(getPosY(), std::min(other.getPosY(), getPosY() + size.y));

    // 计算圆心到最近点的距离
    const float distanceX = other.getPosX() - closestX;
    const float distanceY = other.getPosY() - closestY;

    // 检查距离是否小于圆的半径
    return (distanceX * distanceX + distanceY * distanceY) < (other.getRadius() * other.getRadius());
}

float BoxCollision::getWidth() const {
    return this->size.x;
}

float BoxCollision::getHeight() const {
    return this->size.y;
}

float BoxCollision::getPosX() const {
    return this->position.x + this->offset.x;
}

float BoxCollision::getPosY() const {
    return this->position.y + this->offset.y;
}

void BoxCollision::setPosition(const float x, const float y) {
    this->position = eng::Vec2f(x, y);
}

void BoxCollision::setSize(const float width_, const float height_) {
    this->size = eng::Vec2f(width_, height_);
}
