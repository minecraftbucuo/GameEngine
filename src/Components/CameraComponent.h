//
// Created by MINEC on 2025/12/18.
//

#pragma once
#ifndef SERVER_BUILD
#include "Component.h"

class CameraComponent : public Component {
public:
    CameraComponent() = default;
    void update(const sf::Time& deltaTime) override;
};
#endif
