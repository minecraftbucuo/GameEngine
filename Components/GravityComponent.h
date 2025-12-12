//
// Created by MINEC on 2025/12/9.
//

#pragma once

#include "Component.h"
#include "GameObject.h"
#include "WorldContext.h"

class GravityComponent : public Component {
public:
    GravityComponent() = default;
    void start() override {}
    void update(sf::Time deltaTime) override {
        float worldHeight = WorldContext::getInstance().getWorldHeight();

        if (std::abs(this->owner->getPosition().y + this->owner->getSize().y - worldHeight) < 0.1f
            && std::abs(owner->getSpeed().y) <= 1.f) return;

        std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
        if (!moveComponent) return;
        moveComponent->setSpeedY(owner->getSpeed().y + gravity * deltaTime.asSeconds());

    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(sf::Event& event) override {

    }
    std::string getName() {
        return name;
    }
private:
    float gravity = 3200.f;
    std::string name = "GravityComponent";
};


