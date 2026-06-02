//
// Created by MINEC on 2026/5/14.
//

#pragma once
#include <SFML/Graphics.hpp>
#include "BaseState.h"
#include "Timer.h"

class MarioDeadState : public BaseState {
public:
    MarioDeadState();

    void start() override;

    void update(const sf::Time& deltaTime) override;

#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif

private:
#ifndef SERVER_BUILD
    sf::Sprite sprite;
#endif
    Timer deathTimer;
};
