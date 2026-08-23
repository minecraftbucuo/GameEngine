//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include "Core/Types.h"

class MoveComponent : public Component {
public:
    MoveComponent() = default;
    void update(const eng::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif
    void setPosition(const eng::Vec2f& pos, bool move_collision = true) const;

    void moveCollisionTo(const eng::Vec2f& pos) const;

    void moveCollisionXTo(float posX) const;

    void moveCollisionYTo(float posY) const;

    void setPosition(float posX, float posY, bool move_collision = true) const;

    void setPositionX(float posX, bool move_collision = true) const;

    void setPositionY(float posY, bool move_collision = true) const;

    void addPosition(const eng::Vec2f& pos, bool move_collision = true) const;

    void setSpeed(const eng::Vec2f& speed) const;

    void setSpeedX(float speedX) const;

    void setSpeedY(float speedY) const;

    void setSpeed(float speedX, float speedY) const;

    void addSpeed(const eng::Vec2f& speed) const;
#ifndef SERVER_BUILD
    static void drawArrow(sf::RenderWindow* window, float x1, float y1, float x2, float y2,
                          float arrowSize = 10.0f, eng::Color color = eng::Color::Red);
#endif
private:
    void setCollisionPosition(const eng::Vec2f& pos) const;
};


