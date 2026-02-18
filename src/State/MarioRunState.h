//
// Created by MINEC on 2026/1/29.
//

#pragma once

#include "Animation.h"
#include "FrameManager.h"
#include "BaseState.h"
#include "GameObject.h"
#include "StateMachine.h"

class MarioRunState : public BaseState {
public:
    explicit MarioRunState() : BaseState("MarioRunState") {
        animation_right.setFrames(FrameManager::getInstance().getFrame("right_small_normal"));
        animation_left.setFrames(FrameManager::getInstance().getFrame("left_small_normal"));
    }
    ~MarioRunState() override = default;


    void update(const sf::Time& deltaTime) override {
        if (owner->getSpeed().x == 0.f) {
            owner->getComponent<StateMachine>()->setState("MarioIdleState");
            return;
        }

        const float speedX = owner->getSpeed().x;
        if (speedX > 0.f) {
            setIsLeft(false);
        } else if (speedX < 0.f) {
            setIsLeft(true);
        }

        if (getIsLeft()) {
            animation_left.update(deltaTime);
        } else {
            animation_right.update(deltaTime);
        }

        sf::Sprite* sprite;
        if (getIsLeft()) {
            sprite = &animation_left.getSprite();
        } else {
            sprite = &animation_right.getSprite();
        }
        auto box_collision = owner->getComponent<Collision, BoxCollision>();
        float w = sprite->getGlobalBounds().width;
        float h = sprite->getGlobalBounds().height;
        box_collision->setSize(w, h);
        owner->setSize(w, h);
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
        sf::Sprite* sprite;
        if (getIsLeft()) {
            sprite = &animation_left.getSprite();
        } else {
            sprite = &animation_right.getSprite();
        }
        if (owner) sprite->setPosition(owner->getPosition());
        else std::cout << "MarioRunState: owner is nullptr" << std::endl;

        window->draw(*sprite);
    }

    bool getIsLeft() const {
        return owner->getComponent<StateMachine>()->getIsLeft();
    }

    void setIsLeft(const bool value) const {
        owner->getComponent<StateMachine>()->setIsLeft(value);
    }

private:
    Animation animation_right;
    Animation animation_left;
};
