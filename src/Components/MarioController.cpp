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
#include "Scene.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#ifdef ENGINE_SDL3
#include <SDL3_mixer/SDL_mixer.h>
#endif
#endif

MarioController::~MarioController() {
#ifndef SERVER_BUILD
#ifdef ENGINE_SDL3
    if (jump_track) MIX_DestroyTrack(jump_track);
    if (shoot_track) MIX_DestroyTrack(shoot_track);
#endif
#endif
}

void MarioController::start() {
#ifndef SERVER_BUILD
#ifdef ENGINE_SDL3
    // SDL3 迁移 Step 10：常驻 track 绑定预解码音频（sf::Sound::setBuffer 的等价物）
    auto& am = AssetManager::getInstance();
    jump_track = MIX_CreateTrack(am.getMixer());
    if (jump_track) MIX_SetTrackAudio(jump_track, am.getSoundBuffer("small_jump"));
    shoot_track = MIX_CreateTrack(am.getMixer());
    if (shoot_track) MIX_SetTrackAudio(shoot_track, am.getSoundBuffer("fireball"));
#else
    jump_sound.setBuffer(AssetManager::getInstance().getSoundBuffer("small_jump"));
    shoot_sound.setBuffer(AssetManager::getInstance().getSoundBuffer("fireball"));
#endif
#endif
    jump_timer.setCallback([this]() -> void { this->w_is_pressed = false; });
    shoot_timer.setCallback([&]() -> void { could_shoot = true; });
}

void MarioController::handleEvent(const eng::EngineEvent& event) {
    if (!is_player) return;
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::A) {
            runLeft();
        }
        if (event.key == eng::Key::D) {
            runRight();
        }
        if (event.key == eng::Key::W || event.key == eng::Key::Space) {
            jump();
        }
        if (event.key == eng::Key::J) {
            shoot();
        }
    } else if (event.type == eng::EventType::KeyRelease) {
        if (event.key == eng::Key::A) {
            stopRun();
        }
        if (event.key == eng::Key::D) {
            stopRun();
        }
        if (event.key == eng::Key::W || event.key == eng::Key::Space) {
            w_is_pressed = false;
            auto* nm = owner->getScene()->getNetworkManager();
            if (nm && nm->isClient()) {
                sf::Packet packet;
                packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
                packet << InputType::JumpRelease;
                nm->getClientSocket().append(packet);
            }
        }
    }
}

void MarioController::update(const eng::Time& deltaTime) {
    auto state = owner->getComponent<StateMachine>();
    if (state && state->getCurrentStateName() == "MarioJumpState" && w_is_pressed) {
        jump_timer.update(deltaTime);
    }
    if (w_is_pressed) {
        const auto& move_component = owner->getComponent<MoveComponent>();
        move_component->addSpeed(eng::Vec2f(0.f, -1815.f * deltaTime.asSeconds()));
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
#ifdef ENGINE_SDL3
            if (jump_track) { MIX_StopTrack(jump_track, 0); MIX_PlayTrack(jump_track, 0); }
#else
            jump_sound.stop();
            jump_sound.play();
#endif
            LOG_TRACE("jump sound play!");
        }
#endif
        state->setState("MarioJumpState");
        w_is_pressed = true;
        jump_timer.start(500);

        auto* nm = owner->getScene()->getNetworkManager();
        if (nm && nm->isClient()) {
            sf::Packet packet;
            packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
            packet << InputType::Jump;
            nm->getClientSocket().append(packet);
        }
    }
}

void MarioController::runLeft() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(-CONFIG.game.playerSpeed);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunLeft;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::runRight() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(CONFIG.game.playerSpeed);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunRight;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::stopRun() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(0.f);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::StopRun;
        nm->getClientSocket().append(packet);
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
#ifdef ENGINE_SDL3
        if (shoot_track) { MIX_StopTrack(shoot_track, 0); MIX_PlayTrack(shoot_track, 0); }
#else
        shoot_sound.stop();
        shoot_sound.play();
#endif
        LOG_TRACE("shoot sound play!");
    }
#endif
    auto* network_manager = owner->getScene()->getNetworkManager();
    const auto current_scene = owner->getScene();
    if (network_manager && network_manager->isClient()) {
        sf::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::Shoot;
        network_manager->getClientSocket().append(packet);
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