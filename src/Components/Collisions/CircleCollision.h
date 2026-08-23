//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Collision.h"
#include "Core/Types.h"


class CircleCollision final : public Collision {
public:
    CircleCollision(float x, float y, float radius);
    // void start() override {}
    void update(const eng::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void setPosition(const eng::Vec2f& position) override;
    [[nodiscard]] bool checkCollision(const Collision& other) const override;
    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;
    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision& other) const override;
    [[nodiscard]] float getRadius() const;
    [[nodiscard]] eng::Vec2f getPos() const;
    [[nodiscard]] eng::Vec2f getCollisionPosition() const override;
    [[nodiscard]] float getPosX() const;
    [[nodiscard]] float getPosY() const;
    void setCollisionPosition(const eng::Vec2f& _position) override {
        this->position = _position - offset + eng::Vec2f(radius, radius);
    }
private:
    float radius;
};

