//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Collision.h"


class CircleCollision final : public Collision {
public:
    CircleCollision(float x, float y, float radius);
    // void start() override {}
    void update(const sf::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif
    void setPosition(const sf::Vector2f& position) override;
    [[nodiscard]] bool checkCollision(const Collision& other) const override;
    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;
    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision& other) const override;
    [[nodiscard]] float getRadius() const;
    [[nodiscard]] sf::Vector2f getSize() const override {
        return {radius * 2.f, radius * 2.f};
    }
    [[nodiscard]] sf::Vector2f getPos() const;
    [[nodiscard]] sf::Vector2f getCenter() const override;
    [[nodiscard]] sf::Vector2f getCollisionPosition() const override;
    [[nodiscard]] float getPosX() const;
    [[nodiscard]] float getPosY() const;
    void setCollisionPosition(const sf::Vector2f& _position) override {
        this->position = _position - offset + sf::Vector2f(radius, radius);
    }
private:
    float radius;
};

