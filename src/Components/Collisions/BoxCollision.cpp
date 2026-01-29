//
// Created by MINEC on 2025/12/10.
//

#include "BoxCollision.h"
#include "CircleCollision.h"
#include <iostream>
#include "GameObject.h"

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

void BoxCollision::update(const sf::Time& deltaTime) {
    // this->posX = owner->posX;
    // this->posY = owner->posY;
    // this->position = owner->getPosition();
}

void BoxCollision::render(sf::RenderWindow *window) {
    sf::RectangleShape rect(this->size);
    rect.setPosition(this->position);
    rect.setFillColor(sf::Color::Transparent);
    rect.setOutlineColor(sf::Color::Red);
    rect.setOutlineThickness(2);
    window->draw(rect);
}

void BoxCollision::setPosition(const sf::Vector2f &position) {
    this->position = position;
}

bool BoxCollision::checkCollision(const Collision &other) const {
    return other.checkCollisionWithBox(*this);
}

bool BoxCollision::checkCollisionWithBox(const BoxCollision &other) const {
    const float maxX = std::max(position.x + size.x, other.getPosX() + other.getWidth());
    const float minX = std::min(position.x, other.getPosX());
    const float maxY = std::max(position.y + size.y, other.getPosY() + other.getHeight());
    const float minY = std::min(position.y, other.getPosY());
    return ((maxX - minX < size.x + other.getWidth()) && (maxY - minY < size.y + other.getHeight()));
}

bool BoxCollision::checkCollisionWithCircle(const CircleCollision &other) const {
    // 找到矩形上离圆心最近的点
    const float closestX = std::max(position.x, std::min(other.getPosX(), position.x + size.x));
    const float closestY = std::max(position.y, std::min(other.getPosY(), position.y + size.y));

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
    return this->position.x;
}

float BoxCollision::getPosY() const {
    return this->position.y;
}

void BoxCollision::setPosition(const float x, const float y) {
    this->position = sf::Vector2f(x, y);
}

void BoxCollision::setSize(const float width_, const float height_) {
    this->size = sf::Vector2f(width_, height_);
}

