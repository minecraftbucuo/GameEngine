//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Collision.h"


class CircleCollision final : public Collision {
public:
    CircleCollision(float x, float y, float radius);
    // void start() override {}
    void update(sf::Time deltaTime) override;
    void render(sf::RenderWindow* window) override;
    [[nodiscard]] bool checkCollision(const Collision& other) const override;
    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;
    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision& other) const override;
    [[nodiscard]] float getRadius() const;
    [[nodiscard]] sf::Vector2f getPos() const;
    [[nodiscard]] float getPosX() const;
    [[nodiscard]] float getPosY() const;
private:
    float radius;
    // float posX, posY;
    sf::Vector2f position;
};

