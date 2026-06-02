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
    virtual void setPosition(const sf::Vector2f& _position);

    virtual void setCollisionPosition(const sf::Vector2f& _position);

    virtual void setOffset(const sf::Vector2f& _offset);

    [[nodiscard]] sf::Vector2f getOffset() const;

    // 返回含有偏移量的坐标，真正用于碰撞检测的坐标
    [[nodiscard]] virtual sf::Vector2f getCollisionPosition() const;

    [[nodiscard]] virtual sf::Vector2f getPosition() const;

    [[nodiscard]] virtual bool checkCollision(const Collision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithCircle(const CircleCollision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithBox(const BoxCollision& other) const = 0;

protected:
    sf::Vector2f position;
    sf::Vector2f offset;
};


