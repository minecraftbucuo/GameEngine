//
// Created by MINEC on 2026/1/4.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD

#include "GameObject3D.h"

class Human3D : public GameObject3D {
public:
    Human3D();
    void render(eng::Renderer& renderer) override;

    void update(eng::Time deltaTime) override;
};
#endif
