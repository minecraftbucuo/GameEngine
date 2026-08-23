//
// Created by MINEC on 2026/2/2.
//

#pragma once

#include "Component.h"
#include "Timer.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SFML/Audio.hpp>
#endif

class MarioController : public Component {
public:
    void start() override;

    void handleEvent(const sf::Event& event) override;

    void update(const eng::Time& deltaTime) override;

    void jump(bool play_sound = true);

    void runLeft() const;

    void runRight() const;

    void stopRun() const;

    void setIsPlayer(bool flag);

    void setWisPressed(bool flag);

    void shoot(bool play_sound = true);

private:
#ifndef SERVER_BUILD
    sf::Sound jump_sound;
    sf::Sound shoot_sound;
#endif
    bool w_is_pressed = false;
    Timer jump_timer;
    bool could_shoot = true;
    Timer shoot_timer;
    bool is_player = true;
};

