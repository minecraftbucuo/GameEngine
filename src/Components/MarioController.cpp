//
// Created by MINEC on 2026/5/8.
//

#include "MarioController.h"

#include "GameObject.h"
#include "MoveComponent.h"
#include "StateMachine.h"
#include "ConfigManager.h"
#include "AssetManager.h"
#include "MarioJumpState.h"

void MarioController::handleEvent(const sf::Event& event) {
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::A) {
            runLeft();
        }
        if (event.key.code == sf::Keyboard::D) {
            runRight();
        }
        if (event.key.code == sf::Keyboard::W) {
            jump();
        }
    } else if (event.type == sf::Event::KeyReleased) {
        if (event.key.code == sf::Keyboard::A) {
            stopRun();
        }
        if (event.key.code == sf::Keyboard::D) {
            stopRun();
        }
    }
}

void MarioController::jump(bool play_sound) const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    auto state = owner->getComponent<StateMachine>();
    if (state && state->getCurrentStateName() != "MarioJumpState") {
        moveComponent->setSpeedY(-CONFIG.game.jumpForce);
#ifndef SERVER_BUILD
        if (play_sound) {
            jump_sound.setBuffer(AssetManager::getInstance().getSoundBuffer("small_jump"));
            jump_sound.stop();
            jump_sound.play();
        }
#endif
        state->setState("MarioJumpState");
        std::dynamic_pointer_cast<MarioJumpState>(owner->getComponent<StateMachine>()->getCurrentState())->setJumpTimer();
    }
}

void MarioController::runLeft() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(-CONFIG.game.playerSpeed);
}

void MarioController::runRight() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(CONFIG.game.playerSpeed);
}

void MarioController::stopRun() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(0.f);
}
