//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include "Core/Types.h"

class BoxCollision;
class CircleCollision;

class Collision : public Component {
public:
    Collision() = default;
    virtual void setPosition(const eng::Vec2f& _position);

    virtual void setCollisionPosition(const eng::Vec2f& _position);

    virtual void setOffset(const eng::Vec2f& _offset);

    [[nodiscard]] eng::Vec2f getOffset() const;

    // 返回含有偏移量的坐标，真正用于碰撞检测的坐标
    [[nodiscard]] virtual eng::Vec2f getCollisionPosition() const;

    [[nodiscard]] virtual eng::Vec2f getPosition() const;

    [[nodiscard]] virtual bool checkCollision(const Collision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithCircle(const CircleCollision& other) const = 0;
    [[nodiscard]] virtual bool checkCollisionWithBox(const BoxCollision& other) const = 0;

protected:
    eng::Vec2f position;
    eng::Vec2f offset;
};


