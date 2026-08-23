//
// Created by MINEC on 2026/1/3.
//

#pragma once
#ifndef SERVER_BUILD
#include "Scene.h"
#include "Cube3D.h"
#include "Cube3DWithController.h"
#include "NewModel3D.h"
#include "Penguin3D.h"
#include "Human3D.h"

class GameScene3D : public Scene {
public:
    explicit GameScene3D(sf::RenderWindow* _window) : Scene(_window, "GameScene3D") {}
    ~GameScene3D() override = default;

    void init() override;

    void handleEvent(const eng::EngineEvent& event) override;

    void exit() override;
};
#endif