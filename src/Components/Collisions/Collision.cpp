//
// Created by MINEC on 2026/6/2.
//

#include "Collision.h"
#include "Core/Types.h"

void Collision::setPosition(const eng::Vec2f& _position) {
    this->position = _position;
}

void Collision::setCollisionPosition(const eng::Vec2f& _position) {
    this->position = _position - offset;
}

void Collision::setOffset(const eng::Vec2f& _offset) {
    this->offset = _offset;
}

eng::Vec2f Collision::getOffset() const {
    return offset;
}

eng::Vec2f Collision::getCollisionPosition() const {
    return position + offset;
}

eng::Vec2f Collision::getPosition() const {
    return position;
}
