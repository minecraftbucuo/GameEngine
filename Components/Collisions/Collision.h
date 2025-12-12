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
    void update(sf::Time deltaTime) override {

    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(sf::Event& event) override {

    }
    virtual void setPosition(const sf::Vector2f& position) = 0;
    [[nodiscard]] virtual bool checkCollision(const Collision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithCircle(const CircleCollision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithBox(const BoxCollision& other) const = 0;
};


