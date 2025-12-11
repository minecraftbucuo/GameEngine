//
// Created by MINEC on 2025/12/10.
//

#include "CircleCollision.h"
#include "BoxCollision.h"
#include "GameObject.h"
#include <cmath>

CircleCollision::CircleCollision(float x, float y, float radius) {
    // this->posX = x;
    // this->posY = y;
    this->position = sf::Vector2f(x, y);
    this->radius = radius;
}

void CircleCollision::update(sf::Time deltaTime) {
    // this->posX = owner->posX + owner->width / 2;
    // this->posY = owner->posY + owner->height / 2;
    this->position = owner->position + owner->size * 0.5f;
}

void CircleCollision::render(sf::RenderWindow *window) {
    sf::CircleShape shape(radius);
    shape.setPosition(position - sf::Vector2f(radius, radius));
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color::Red);
    shape.setOutlineThickness(1.f);
    window->draw(shape);
}

bool CircleCollision::checkCollision(const Collision &other) const {
    return other.checkCollisionWithCircle(*this);
}

bool CircleCollision::checkCollisionWithBox(const BoxCollision &other) const {
    return other.checkCollisionWithCircle(*this);
}


bool CircleCollision::checkCollisionWithCircle(const CircleCollision &other) const {
    const float distance = sqrtf(powf(other.getPos().x - position.x, 2) + powf(other.getPos().y - position.y, 2));
    return distance < radius + other.getRadius();
}

float CircleCollision::getRadius() const {
    return radius;
}

sf::Vector2f CircleCollision::getPos() const {
    return this->position;
}

float CircleCollision::getPosX() const {
    return this->position.x;
}

float CircleCollision::getPosY() const {
    return this->position.y;
}

