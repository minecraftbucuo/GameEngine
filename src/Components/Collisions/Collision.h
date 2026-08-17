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

    // 返回碰撞体左上角（含偏移量），所有形状统一语义（B3）：
    // BoxCollision = 左上角；CircleCollision = 包围盒左上角（圆心 - 半径）。
    // 这是碰撞检测/事件快照统一使用的坐标。
    [[nodiscard]] virtual sf::Vector2f getCollisionPosition() const;

    // 返回碰撞体中心（B3 统一语义）：
    // BoxCollision = 左上角 + 半尺寸；CircleCollision = 圆心。
    [[nodiscard]] virtual sf::Vector2f getCenter() const;

    // 返回内部存储位置（各形状语义不同：Box = 左上角、Circle = 圆心）。
    // 不建议外部使用，请用 getCollisionPosition()/getCenter()。
    [[nodiscard]] virtual sf::Vector2f getPosition() const;

    [[nodiscard]] virtual bool checkCollision(const Collision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithCircle(const CircleCollision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithBox(const BoxCollision& other) const = 0;

protected:
    sf::Vector2f position;
    sf::Vector2f offset;
};


