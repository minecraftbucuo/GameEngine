//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include "Core/Types.h"
#include "Core/Event.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"

class Camera {
public:
    Camera() = default;
    explicit Camera(eng::Renderer* renderer);

    void init(eng::Renderer* _renderer);

    void init();

    void resize();

    void setSize(float width, float height);

    void setPosition(float x, float y);

    eng::Vec2f getPosition() const;

    void setPositionX(float x);

    void setMouseControl(bool flag);

    eng::Vec2f getCenter() const;

    void addPosition(const eng::Vec2i& pos);

    void handleEvent(const eng::EngineEvent& event);

    eng::Vec2f getViewSize() const;

private:
    // left/top 语义为可视区左上角（与原 sf::View(FloatRect) 构造语义一致）
    eng::FloatRect floatRect;
    eng::Renderer* renderer{};
    bool mouseControl = false;
    eng::Vec2i mousePos;
    bool isPressed = false;

    void updateView();
};
#endif
