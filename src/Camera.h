//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SFML/Graphics/RenderWindow.hpp>

class Camera {
public:
    Camera() = default;
    explicit Camera(sf::RenderWindow* window);

    void init(sf::RenderWindow* _window);

    void init();

    void resize();

    void setSize(float width, float height);

    void setPosition(float x, float y);

    eng::Vec2f getPosition() const;

    void setPositionX(float x);

    void setMouseControl(bool flag);

    eng::Vec2f getCenter() const;

    void addPosition(const eng::Vec2i& pos);

    void handleEvent(const sf::Event& event);

    eng::Vec2f getViewSize() const;

private:
    eng::FloatRect floatRect;
    sf::View view;
    sf::RenderWindow* window{};
    bool mouseControl = false;
    eng::Vec2i mousePos;
    bool isPressed = false;

    void updateView();
};
#endif
