//
// Created by MINEC on 2026/2/2.
//

#pragma once

#include "Component.h"
#ifndef SERVER_BUILD
#include <SFML/Audio.hpp>
#endif

class MarioController : public Component {
public:
    void handleEvent(const sf::Event& event) override;

    void jump(bool play_sound = true) const;

    void runLeft() const;

    void runRight() const;

    void stopRun() const;

private:
#ifndef SERVER_BUILD
    mutable sf::Sound jump_sound;
#endif
};

