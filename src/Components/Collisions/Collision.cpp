//
// Created by MINEC on 2026/6/2.
//

#include "Collision.h"

void Collision::setPosition(const sf::Vector2f& _position) {
    this->position = _position;
}

void Collision::setCollisionPosition(const sf::Vector2f& _position) {
    this->position = _position - offset;
}

void Collision::setOffset(const sf::Vector2f& _offset) {
    this->offset = _offset;
}

sf::Vector2f Collision::getOffset() const {
    return offset;
}

sf::Vector2f Collision::getCollisionPosition() const {
    return position + offset;
}

// 基类默认把左上角当作中心，派生类（Box/Circle）应 override 为真实形状中心
sf::Vector2f Collision::getCenter() const {
    return getCollisionPosition();
}

sf::Vector2f Collision::getPosition() const {
    return position;
}
