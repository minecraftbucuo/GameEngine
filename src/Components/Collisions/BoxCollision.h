//
// Created by MINEC on 2025/12/10.
//


#pragma once
#include "Collision.h"
#include "CircleCollision.h"

class BoxCollision : public Collision {
public:
    BoxCollision(float x, float y, float width, float height);
    BoxCollision();
    void start() override;
    void update(const sf::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif

    [[nodiscard]] bool checkCollision(const Collision& other) const override;

    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;

    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision &other) const override;


    [[nodiscard]] float getWidth() const;

    [[nodiscard]] float getHeight() const;

    // B3：中心 = 左上角 + 半尺寸
    [[nodiscard]] sf::Vector2f getCenter() const override;

    [[nodiscard]] float getPosX() const;

    [[nodiscard]] float getPosY() const;

    void setPosition(float x, float y);

    void setSize(float width_, float height_);

private:
    sf::Vector2f size;
};





