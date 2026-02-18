//
// Created by MINEC on 2026/1/30.
//

#pragma once

#include "AssetManager.h"
#include "BaseState.h"
#include "GameObject.h"
#include "StateMachine.h"


class MarioIdleState : public BaseState {
public:
    explicit MarioIdleState() : BaseState("MarioIdleState") {
        const sf::Texture& mario_texture = AssetManager::getInstance().getTexture("mario_bros");
        right_sprite.setTexture(mario_texture);
        right_sprite.setTextureRect(sf::IntRect(178, 32, 12, 16));
        right_sprite.setScale(4.f, 4.f);
        left_sprite.setTexture(mario_texture);
        left_sprite.setTextureRect(sf::IntRect(178, 32, 12, 16));
        left_sprite.setScale(-4.f, 4.f);
        left_sprite.setOrigin(static_cast<float>(right_sprite.getTextureRect().width), 0.f);
    }
    ~MarioIdleState() override = default;

    void start() override {
        const auto box_collision = owner->getComponent<Collision, BoxCollision>();
        const float w = left_sprite.getGlobalBounds().width;
        const float h = left_sprite.getGlobalBounds().height;
        box_collision->setSize(w, h);
        owner->setSize(w, h);
    }

    void update(const sf::Time& deltaTime) override {
        if (owner->getSpeed().x != 0.f) {
            owner->getComponent<StateMachine>()->setState("MarioRunState");
        }
    }

    void handleEvent(const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::A) {
                setIsLeft(true);
            } else if (event.key.code == sf::Keyboard::D) {
                setIsLeft(false);
            } else if (event.key.code == sf::Keyboard::W) {
                owner->getComponent<StateMachine>()->setState("MarioJumpState");
            }
        }
    }

    void render(sf::RenderWindow* window) override {
        if (getIsLeft()) {
            if (owner) left_sprite.setPosition(owner->getPosition());
            else std::cout << "MarioRunState: owner is nullptr" << std::endl;
            window->draw(left_sprite);
        } else {
            if (owner) right_sprite.setPosition(owner->getPosition());
            else std::cout << "MarioRunState: owner is nullptr" << std::endl;
            window->draw(right_sprite);
        }
    }

    bool getIsLeft() const {
        return owner->getComponent<StateMachine>()->getIsLeft();
    }

    void setIsLeft(const bool value) const {
        owner->getComponent<StateMachine>()->setIsLeft(value);
    }

private:
    sf::Sprite left_sprite;
    sf::Sprite right_sprite;
};
