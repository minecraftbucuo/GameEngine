//
// Created by MINEC on 2026/1/3.
//

#pragma once

#include "Scene.h"
#include "Cube.h"
#include "NewModel.h"
#include "Penguin.h"
#include "Human.h"

class GameScene3D : public Scene {
public:
    explicit GameScene3D(sf::RenderWindow* _window) : Scene(_window, "GameScene3D") {}
    ~GameScene3D() override = default;

    void init() override {
        window->setSize(sf::Vector2u(1200, 1200));
        Scene::init();
        // this->addObject(std::make_shared<Penguin>());
        this->addObject(std::make_shared<Cube>());
        // this->addObject(std::make_shared<NewModel>());
        // this->addObject(std::make_shared<Human>());
    }
};
