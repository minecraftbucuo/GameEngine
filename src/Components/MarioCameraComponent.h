//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Component.h"

class MarioCameraComponent : public Component {
public:
    MarioCameraComponent() = default;

    void start() override;

    void update(const eng::Time& deltaTime) override;

    void setTargetPosition(const eng::Vec2f& pos);

    void setTargetPositionX(float x);

private:
    eng::Vec2f target_position;
    eng::Vec2f position;
};
#endif
