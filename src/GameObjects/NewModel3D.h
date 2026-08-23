//
// Created by MINEC on 2026/1/4.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "GameObject3D.h"

class NewModel3D : public GameObject3D {
public:
    NewModel3D();
    void render(sf::RenderWindow* window) override;

    void update(eng::Time deltaTime) override;
};
#endif
