//
// Created by MINEC on 2026/3/14.
//

#pragma once

#include "BoxGameObject.h"

class Brick : public BoxGameObject {
public:
    Brick(float x, float y, const std::string& tag = "brick");

    void setPosition(float posX, float posY) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override {
        BoxGameObject::render(window);
        window->draw(sprite);
    }
#endif
private:
#ifndef SERVER_BUILD
    sf::Sprite sprite;
#endif
};