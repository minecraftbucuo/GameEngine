//
// Created by MINEC on 2026/1/29.
//

#pragma once

#include "Animation.h"
#include "AnimationManager.h"
#include "BaseState.h"
#include "GameObject.h"

class MarioRunState : public BaseState {
public:
    explicit MarioRunState() : BaseState("MarioRunState") {};
    ~MarioRunState() override = default;

    void start() override {
        animation_right = AnimationManager::getInstance().getAnimation("right_small_normal");
        animation_left = AnimationManager::getInstance().getAnimation("left_small_normal");
    }
    void update(const sf::Time& deltaTime) override {
        if (isLeft) {
            animation_left.update(deltaTime);
        } else {
            animation_right.update(deltaTime);
        }

        sf::Sprite* sprite;
        if (isLeft) {
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
                isLeft = true;
            } else if (event.key.code == sf::Keyboard::D) {
                isLeft = false;
            }
        }
    }
    void render(sf::RenderWindow* window) override {
        sf::Sprite* sprite;
        if (isLeft) {
            sprite = &animation_left.getSprite();
        } else {
            sprite = &animation_right.getSprite();
        }
        if (owner) sprite->setPosition(owner->getPosition());
        else std::cout << "MarioRunState: owner is nullptr" << std::endl;

        window->draw(*sprite);
    }

private:
    Animation animation_right;
    Animation animation_left;
    bool isLeft = true;
};