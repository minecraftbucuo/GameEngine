//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"

class BoxCollision;
class CircleCollision;

class Collision : public Component {
public:
    Collision() = default;
    void start() override {

    }
    void update(const sf::Time& deltaTime) override {

    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(const sf::Event& event) override {

    }
    virtual void setPosition(const sf::Vector2f& _position) {
        this->position = _position;
    }

    [[nodiscard]] virtual sf::Vector2f getPosition() const {
        return position;
    }

    [[nodiscard]] virtual bool checkCollision(const Collision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithCircle(const CircleCollision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithBox(const BoxCollision& other) const = 0;

protected:
    sf::Vector2f position;
};


