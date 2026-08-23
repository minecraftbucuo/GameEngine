//
// Created by MINEC on 2026/3/25.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD

#include "Cube3D.h"

class Cube3DWithController : public Cube3D {
public:
    Cube3DWithController();

    void start() override;

    void update(const eng::Time deltaTime) override {
    }

    void handleEvent(sf::Event& event) override;

private:
    bool mouse_is_pressed{};
    eng::Vec2i mousePos;
};
#endif