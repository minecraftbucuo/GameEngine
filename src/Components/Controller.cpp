//
// Created by MINEC on 2026/6/2.
//

#include "Controller.h"
#include <memory>

#include "GameObject.h"
#include "MoveComponent.h"

void Controller::handleEvent(const sf::Event& event) {
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
            moveComponent->setSpeedY(-1200.f);
        }
    }
}
