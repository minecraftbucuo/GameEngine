//
// Created by MINEC on 2026/3/25.
//

#pragma once
#ifndef SERVER_BUILD

#include "Cube3D.h"

class Cube3DWithController : public Cube3D {
public:
    Cube3DWithController();

    void start() override;

    void update(const sf::Time deltaTime) override {
    }

    void handleEvent(sf::Event& event) override;

private:
    bool mouse_is_pressed{};
    sf::Vector2i mousePos;
};
#endif