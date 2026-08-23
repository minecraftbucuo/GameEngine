//
// Created by MINEC on 2026/2/4.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "GameObject.h"

namespace eng { class Renderer; }

class Circle : public GameObject {
public:
    Circle(float x, float y, float radius, const std::string& tag = "circle");

    ~Circle() override;

    void render(eng::Renderer& renderer) override;

    void start() override;

    void update(eng::Time deltaTime) override;

    bool needGravity();

private:
    void setPosition(float x, float y) override;

    void setSpeed(float x, float y);
};
#endif

