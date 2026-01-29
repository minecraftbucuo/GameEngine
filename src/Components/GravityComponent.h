//
// Created by MINEC on 2025/12/9.
//

#pragma once

#include "Component.h"
#include "GameObject.h"
#include "SceneContext.h"
#include "MoveComponent.h"

class GravityComponent : public Component {
public:
    GravityComponent() = default;
    void start() override {}
    void update(const sf::Time& deltaTime) override {
        float worldHeight = SceneContext::getInstance().getWindowHeight();

        if (std::abs(this->owner->getPosition().y + this->owner->getSize().y - worldHeight) < 0.1f
            && std::abs(owner->getSpeed().y) <= 1.f) return;

        std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
        if (!moveComponent) return;
        moveComponent->setSpeedY(owner->getSpeed().y + gravity * deltaTime.asSeconds());

    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(const sf::Event& event) override {

    }
    std::string getName() {
        return name;
    }
private:
    float gravity = 3200.f;
    std::string name = "GravityComponent";
};


