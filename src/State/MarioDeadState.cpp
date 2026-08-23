//
// Created by MINEC on 2026/6/2.
//

#include "MarioDeadState.h"
#ifndef SERVER_BUILD
#include "AssetManager.h"
#endif
#include "Collision.h"
#include "GravityComponent.h"
#include "MoveComponent.h"
#include "GameObject.h"
#include "Core/Types.h"

MarioDeadState::MarioDeadState() : BaseState("MarioDeadState") {
#ifndef SERVER_BUILD
    const sf::Texture& mario_texture = AssetManager::getInstance().getTexture("mario_bros");
    sprite.setTexture(mario_texture);
    sprite.setTextureRect(eng::IntRect(160, 32, 16, 16));
    sprite.setScale(4.f, 4.f);
#endif
}

void MarioDeadState::start() {
    if (const auto collision = owner->getComponent<Collision>()) collision->setActive(false);
    if (const auto gravity = owner->getComponent<GravityComponent>()) gravity->setActive(true);
    if (const auto move = owner->getComponent<MoveComponent>()) {
        move->setSpeed(eng::Vec2f(0.f, -500.f));
    }
    deathTimer.setCallback(
        [this]() -> void {
            owner->destroy();
            LOG_DEBUG("MarioDeadState destroy");
        }
    );
    deathTimer.start(600);
    LOG_DEBUG("MarioDeadState start");
}

void MarioDeadState::update(const eng::Time& deltaTime) {
    deathTimer.update(deltaTime);
}

#ifndef SERVER_BUILD
void MarioDeadState::render(sf::RenderWindow* window) {
    if (owner) {
        sprite.setPosition(owner->getPosition());
        window->draw(sprite);
    }
}
#endif
