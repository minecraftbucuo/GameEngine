//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include <SFML/Graphics.hpp>
#include "GameObject.h"
#include "MoveComponent.h"

class Controller : public Component {
public:
    void start() override {}
    void update(const sf::Time& deltaTime) override {}
    void render(sf::RenderWindow* window) override {}
    void handleEvent(const sf::Event& event) override {
        std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
        if (!moveComponent) {
            moveComponent = owner->addComponent<MoveComponent>();
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::A) {
                moveComponent->setSpeedX(-500.f);
            }
            if (event.key.code == sf::Keyboard::D) {
                moveComponent->setSpeedX(500.f);
            }
            if (event.key.code == sf::Keyboard::W) {
                moveComponent->setSpeedY(-900.f);
            }
        } else if (event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::A) {
                moveComponent->setSpeedX(0.f);
            }
            if (event.key.code == sf::Keyboard::D) {
                moveComponent->setSpeedX(0.f);
            }
        }
    }

};

