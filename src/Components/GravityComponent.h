//
// Created by MINEC on 2025/12/9.
//

#pragma once

#include "Component.h"
#include "ConfigManager.h"

class GravityComponent : public Component {
public:
    GravityComponent() = default;
    void update(const sf::Time& deltaTime) override;
    std::string getName();
private:
    float gravity = CONFIG.game.gravity;
    std::string name = "GravityComponent";
};


