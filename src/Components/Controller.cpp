//
// Created by MINEC on 2026/6/2.
//

#include "Controller.h"
#include <memory>

#include "GameObject.h"
#include "MoveComponent.h"

void Controller::handleEvent(const eng::EngineEvent& event) {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::A) {
            moveComponent->setSpeedX(-500.f);
        }
        if (event.key == eng::Key::D) {
            moveComponent->setSpeedX(500.f);
        }
        if (event.key == eng::Key::W) {
            moveComponent->setSpeedY(-1200.f);
        }
    }
}
