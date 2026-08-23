//
// Created by MINEC on 2025/12/10.
//


#pragma once
#include "Collision.h"
#include "CircleCollision.h"
#include "Core/Types.h"

class BoxCollision : public Collision {
public:
    BoxCollision(float x, float y, float width, float height);
    BoxCollision();
    void start() override;
    void update(const eng::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif

    [[nodiscard]] bool checkCollision(const Collision& other) const override;

    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;

    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision &other) const override;


    [[nodiscard]] float getWidth() const;

    [[nodiscard]] float getHeight() const;

    [[nodiscard]] float getPosX() const;

    [[nodiscard]] float getPosY() const;

    void setPosition(float x, float y);

    void setSize(float width_, float height_);

private:
    eng::Vec2f size;
};





