//
// Created by MINEC on 2026/2/2.
//

#pragma once

#include "Component.h"
#include <SFML/Graphics.hpp>
#include "GameObject.h"
#include "MoveComponent.h"
#include "StateMachine.h"

class MarioController : public Component {
public:
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
                auto state = owner->getComponent<StateMachine>();
                if (state && state->getCurrentStateName() != "MarioJumpState")
                    moveComponent->setSpeedY(-1200.f);
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

