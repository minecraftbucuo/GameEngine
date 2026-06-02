//
// Created by MINEC on 2026/2/4.
//

#pragma once
#ifndef SERVER_BUILD
#include "GameObject.h"
#include <SFML/Graphics.hpp>

#include "FrameManager.h"
#include "AssetManager.h"


class Circle : public GameObject {
public:
    Circle(float x, float y, float radius, const std::string& tag = "circle");

    ~Circle() override;

    void render(sf::RenderWindow* window) override;

    void start() override;

    void update(sf::Time deltaTime) override;

    bool needGravity();

private:
    void setPosition(float x, float y) override;

    void setSpeed(float x, float y);

    sf::CircleShape shape;
};
#endif

