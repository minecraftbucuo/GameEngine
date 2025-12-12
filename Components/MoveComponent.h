//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include "GameObject.h"

class MoveComponent : public Component {
public:
    MoveComponent() = default;
    void start() override {}
    void update(sf::Time deltaTime) override {
        owner->position += owner->speed * deltaTime.asSeconds();
        setPosition(owner->position.x, owner->position.y);
    }
    void render(sf::RenderWindow* window) override {

    }

    void handleEvent(sf::Event& event) override {

    }
    void setPosition(const sf::Vector2f& pos) const {
        owner->setPosition(pos.x, pos.y);
    }

    void setPosition(const float posX, const float posY) const {
        owner->setPosition(posX, posY);
    }

    void setPositionX(const float posX) const {
        setPosition(posX, owner->position.y);
    }

    void setPositionY(const float posY) const {
        setPosition(owner->position.x, posY);
    }

    void setSpeed(const sf::Vector2f& speed) const {
        owner->speed = speed;
    }

    void setSpeedX(const float speedX) const {
        owner->speed.x = speedX;
    }

    void setSpeedY(const float speedY) const {
        owner->speed.y = speedY;
    }

    void setSpeed(const float speedX, const float speedY) const {
        owner->speed = sf::Vector2f(speedX, speedY);
    }
};


