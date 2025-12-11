//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include <SFML/Graphics.hpp>
#include "GameObject.h"

class Controller : public Component {
public:
    void start() override {}
    void update(sf::Time deltaTime) override {}
    void render(sf::RenderWindow* window) override {}
    void handleEvent(sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::A) {
                owner->speed.x = -500;
            }
            if (event.key.code == sf::Keyboard::D) {
                owner->speed.x = 500;
            }
            if (event.key.code == sf::Keyboard::W) {
                owner->speed.y = -900;
            }
        }
    }

};

