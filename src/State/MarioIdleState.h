//
// Created by MINEC on 2026/1/30.
//

#pragma once
#include "BaseState.h"
#ifndef SERVER_BUILD
#include <SFML/Audio.hpp>
#endif

class MarioIdleState : public BaseState {
public:
    explicit MarioIdleState();
    ~MarioIdleState() override = default;

    void start() override;

    void update(const sf::Time& deltaTime) override;

    void handleEvent(const sf::Event& event) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif
    bool getIsLeft() const;

    void setIsLeft(bool value) const;

private:
#ifndef SERVER_BUILD
    sf::Sprite left_sprite;
    sf::Sprite right_sprite;
    sf::Sound jump_sound;
#endif
};
