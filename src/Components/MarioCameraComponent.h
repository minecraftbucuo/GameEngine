//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include "Component.h"
#include "SceneContext.h"
#include "Camera.h"
#include "GameObject.h"

class MarioCameraComponent : public Component {
public:
    MarioCameraComponent() = default;
    void start() override {

    }
    void update(const sf::Time& deltaTime) override {
        if (Camera* camera = SceneContext::getInstance().getCamera()) {
            // camera->setPosition(owner->getPosition().x - 400, owner->getPosition().y - 600);
            if (owner->getPosition().x > 200) camera->setPositionX(owner->getPosition().x - 200);
        }
    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(const sf::Event& event) override {

    }
};
