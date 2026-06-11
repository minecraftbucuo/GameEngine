//
// Created by MINEC on 2026/5/8.
//

#include "MarioController.h"

#include "GameObject.h"
#include "MoveComponent.h"
#include "StateMachine.h"
#include "ConfigManager.h"
#include "AssetManager.h"
#include "FireBall.h"
#include "MarioJumpState.h"
#include "NetworkManager.h"
#include "SceneContext.h"
#include "SceneManager.h"

void MarioController::start() {
#ifndef SERVER_BUILD
    jump_sound.setBuffer(AssetManager::getInstance().getSoundBuffer("small_jump"));
    shoot_sound.setBuffer(AssetManager::getInstance().getSoundBuffer("fireball"));
#endif
    jump_timer.setCallback([this]() -> void { this->w_is_pressed = false; });
    shoot_timer.setCallback([&]() -> void { could_shoot = true; });
}

void MarioController::handleEvent(const sf::Event& event) {
    if (!is_player) return;
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::A) {
            runLeft();
        }
        if (event.key.code == sf::Keyboard::D) {
            runRight();
        }
        if (event.key.code == sf::Keyboard::W || event.key.code == sf::Keyboard::Space) {
            jump();
        }
        if (event.key.code == sf::Keyboard::J) {
            shoot();
        }
    } else if (event.type == sf::Event::KeyReleased) {
        if (event.key.code == sf::Keyboard::A) {
            stopRun();
        }
        if (event.key.code == sf::Keyboard::D) {
            stopRun();
        }
        if (event.key.code == sf::Keyboard::W || event.key.code == sf::Keyboard::Space) {
            w_is_pressed = false;
            if (SceneContext::getInstance().getNetworkManager()->isClient()) {
                sf::Packet packet;
                packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
                packet << InputType::JumpRelease;
                SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
            }
        }
    }
}

void MarioController::update(const sf::Time& deltaTime) {
    auto state = owner->getComponent<StateMachine>();
    if (state && state->getCurrentStateName() == "MarioJumpState" && w_is_pressed) {
        jump_timer.update(deltaTime);
    }
    if (w_is_pressed) {
        const auto& move_component = owner->getComponent<MoveComponent>();
        move_component->addSpeed(sf::Vector2f(0.f, -1815.f * deltaTime.asSeconds()));
    }

    shoot_timer.update(deltaTime);
}

void MarioController::jump(const bool play_sound) {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    auto state = owner->getComponent<StateMachine>();
    if (state && state->getCurrentStateName() != "MarioJumpState") {
        if (state->getCurrentStateName() == "MarioDeadState") return;
        moveComponent->setSpeedY(-CONFIG.game.jumpForce);
#ifndef SERVER_BUILD
        if (play_sound) {
            jump_sound.stop();
            jump_sound.play();
            LOG_TRACE("jump sound play!");
        }
#endif
        state->setState("MarioJumpState");
        w_is_pressed = true;
        jump_timer.start(500);

        if (SceneContext::getInstance().getNetworkManager()->isClient()) {
            sf::Packet packet;
            packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
            packet << InputType::Jump;
            SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
        }
    }
}

void MarioController::runLeft() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(-CONFIG.game.playerSpeed);

    if (SceneContext::getInstance().getNetworkManager()->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunLeft;
        SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
    }
}

void MarioController::runRight() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(CONFIG.game.playerSpeed);

    if (SceneContext::getInstance().getNetworkManager()->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunRight;
        SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
    }
}

void MarioController::stopRun() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(0.f);

    if (SceneContext::getInstance().getNetworkManager()->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::StopRun;
        SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
    }
}

void MarioController::setIsPlayer(const bool flag) {
    is_player = flag;
}

void MarioController::setWisPressed(const bool flag) {
    w_is_pressed = flag;
}

void MarioController::shoot(const bool play_sound) {
    if (!could_shoot) return;
    could_shoot = false;
    shoot_timer.start(CONFIG.game.shootDelay);
#ifndef SERVER_BUILD
    if (play_sound) {
        shoot_sound.stop();
        shoot_sound.play();
        LOG_TRACE("shoot sound play!");
    }
#endif
    const auto network_manager = SceneContext::getInstance().getNetworkManager();
    const auto current_scene = SceneContext::getInstance().getSceneManager()->getCurrentScene();
    if (network_manager->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::Shoot;
        SceneContext::getInstance().getNetworkManager()->getClientSocket().append(packet);
        return;
    }
    if (owner->getComponent<StateMachine>()->getIsLeft()) {
        current_scene->addObjectWithNetwork(std::make_shared<FireBall>(owner->getId(),
            owner->getPosition().x - 32.f, owner->getPosition().y, -600.f));
    }
    else {
        current_scene->addObjectWithNetwork(std::make_shared<FireBall>(owner->getId(), owner->getPosition().x +
            owner->getComponent<Collision>()->getOffset().x + owner->getSize().x, owner->getPosition().y, 600.f));
    }
}
