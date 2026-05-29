//
// Created by MINEC on 2026/5/14.
//

#pragma once
#ifndef SERVER_BUILD
#include "AssetManager.h"
#endif
#include "BaseState.h"
#include "Timer.h"
#include "GameObject.h"
#include "GravityComponent.h"


class MarioDeadState : public BaseState {
public:
    MarioDeadState() : BaseState("MarioDeadState") {
#ifndef SERVER_BUILD
        const sf::Texture& mario_texture = AssetManager::getInstance().getTexture("mario_bros");
        sprite.setTexture(mario_texture);
        sprite.setTextureRect(sf::IntRect(160, 32, 16, 16));
        sprite.setScale(4.f, 4.f);
#endif
    }

    void start() override {
        if (const auto collision = owner->getComponent<Collision>()) collision->setActive(false);
        if (const auto gravity = owner->getComponent<GravityComponent>()) gravity->setActive(true);
        if (const auto move = owner->getComponent<MoveComponent>()) {
            move->setSpeed(sf::Vector2f(0.f, -500.f));
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

    void update(const sf::Time& deltaTime) override {
        deathTimer.update(deltaTime);
    }

#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override {
        if (owner) {
            sprite.setPosition(owner->getPosition());
            window->draw(sprite);
        }
    }
#endif

private:
#ifndef SERVER_BUILD
    sf::Sprite sprite;
#endif
    Timer deathTimer;
};
