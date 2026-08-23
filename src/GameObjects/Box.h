//
// Created by MINEC on 2026/3/18.
//

#pragma once
#include "Animation.h"
#include "BoxGameObject.h"
#include "Core/Types.h"

class Box : public BoxGameObject {
public:
    Box(float x, float y, const std::string& tag = "box");

    ~Box() override;

    void start() override;

    void update(eng::Time deltaTime) override;

    void setPosition(float posX, float posY) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

private:
#ifndef SERVER_BUILD
    Animation animation;
#endif
    float last_y;
};
