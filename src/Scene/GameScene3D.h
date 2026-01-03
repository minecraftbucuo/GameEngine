//
// Created by MINEC on 2026/1/3.
//

#pragma once

#include "Scene.h"
#include "Cube.h"

class GameScene3D : public Scene {
public:
    GameScene3D(sf::RenderWindow* _window) : Scene(_window) {}
    ~GameScene3D() override = default;

    void init() override {
        window->setSize(sf::Vector2u(800, 800));
        this->setCamera(window);
        this->setSceneContext();
        this->addObject(std::make_shared<Cube>());
    }
};
