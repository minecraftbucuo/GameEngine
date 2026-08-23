//
// Created by MINEC on 2026/1/30.
//

#pragma once
#ifndef SERVER_BUILD
#include "AssetManager.h"
#endif
#include "BaseState.h"
#include "BoxCollision.h"
#include "Collision.h"
#include "GameObject.h"
#include "StateMachine.h"
#include "GravityComponent.h"
#include "Timer.h"
#include "MoveComponent.h"
#include "Core/Types.h"


class MarioJumpState : public BaseState {
public:
    MarioJumpState() : BaseState("MarioJumpState") {
#ifndef SERVER_BUILD
        const sf::Texture& mario_texture = AssetManager::getInstance().getTexture("mario_bros");
        right_sprite.setTexture(mario_texture);
        right_sprite.setTextureRect(eng::IntRect(144, 32, 16, 16));
        right_sprite.setScale(4.f, 4.f);
        left_sprite.setTexture(mario_texture);
        left_sprite.setTextureRect(eng::IntRect(144, 32, 16, 16));
        left_sprite.setScale(-4.f, 4.f);
        left_sprite.setOrigin(static_cast<float>(right_sprite.getTextureRect().width), 0.f);
#endif
    }
    ~MarioJumpState() override = default;

    void update(const eng::Time& deltaTime) override {
        if (owner->getSpeed().x < 0) {
            setIsLeft(true);
        } else if (owner->getSpeed().x > 0) {
            setIsLeft(false);
        }
        const auto& box_collision = owner->getComponent<Collision, BoxCollision>();
        if (!getIsLeft()) {
            box_collision->setOffset(eng::Vec2f(16.f, 0.f));
        } else {
            box_collision->setOffset(eng::Vec2f(0.f, 0.f));
        }
    }
#ifndef SERVER_BUILD
    void handleEvent(const eng::EngineEvent& event) override {
        if (event.type == eng::EventType::KeyPress) {
            if (event.key == eng::Key::A) {
                setIsLeft(true);
            } else if (event.key == eng::Key::D) {
                setIsLeft(false);
            }
        }
    }

    void render(sf::RenderWindow* window) override {
        if (getIsLeft()) {
            if (owner) left_sprite.setPosition(owner->getPosition());
            else LOG_ERROR("Owner is nullptr");
            window->draw(left_sprite);
        } else {
            if (owner) right_sprite.setPosition(owner->getPosition());
            else LOG_ERROR("Owner is nullptr");
            window->draw(right_sprite);
        }
    }
#endif
    bool getIsLeft() const {
        return owner->getComponent<StateMachine>()->getIsLeft();
    }

    void setIsLeft(const bool value) const {
        owner->getComponent<StateMachine>()->setIsLeft(value);
    }

private:
#ifndef SERVER_BUILD
    sf::Sprite left_sprite;
    sf::Sprite right_sprite;
#endif
};
