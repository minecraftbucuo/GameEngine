//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include "GameObject.h"

class MoveComponent : public Component {
public:
    MoveComponent() = default;
    void start() override {}
    void update(sf::Time deltaTime) override {
        owner->position += owner->speed * deltaTime.asSeconds();
        owner->setPosition(owner->position.x, owner->position.y);
    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(sf::Event& event) override {

    }
};


