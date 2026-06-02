//
// Created by MINEC on 2025/12/18.
//

#pragma once
#ifndef SERVER_BUILD
#include <SFML/Graphics/RenderWindow.hpp>

class Camera {
public:
    Camera() = default;
    explicit Camera(sf::RenderWindow* window);

    void init(sf::RenderWindow* _window);

    void init();

    void resize();

    void setSize(const float width, const float height);

    void setPosition(const float x, const float y);

    sf::Vector2f getPosition() const;

    void setPositionX(const float x);

    void setMouseControl(const bool flag);

    sf::Vector2f getCenter() const;

    void addPosition(const sf::Vector2i& pos);

    void handleEvent(const sf::Event& event);

    sf::Vector2f getViewSize() const;

private:
    sf::FloatRect floatRect;
    sf::View view;
    sf::RenderWindow* window{};
    bool mouseControl = true;
    sf::Vector2i mousePos;
    bool isPressed = false;

    void updateView();
};
#endif