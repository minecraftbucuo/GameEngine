//
// Created by MINEC on 2025/12/18.
//

#pragma once
#ifndef SERVER_BUILD
#include "Component.h"

class MarioCameraComponent : public Component {
public:
    MarioCameraComponent() = default;

    void start() override;

    void update(const sf::Time& deltaTime) override;

    void setTargetPosition(const sf::Vector2f& pos);

    void setTargetPositionX(float x);

private:
    sf::Vector2f target_position;
    sf::Vector2f position;
};
#endif
