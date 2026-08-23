//
// Created by MINEC on 2025/12/9.
//

#pragma once

#include "Component.h"
#include "ConfigManager.h"
#include "Core/Types.h"

class GravityComponent : public Component {
public:
    GravityComponent() = default;
    void update(const eng::Time& deltaTime) override;
    std::string getName();
private:
    float gravity = CONFIG.game.gravity;
    std::string name = "GravityComponent";
};


