//
// Created by MINEC on 2026/8/18.
//

#pragma once
#ifndef SERVER_BUILD
#include "Scene.h"

class PhysicsTestScene : public Scene {
public:
    explicit PhysicsTestScene(sf::RenderWindow* _window) : Scene(_window, "PhysicsTestScene") {
        // 启用 Box2D 物理
        usePhysics = true;
    }
    ~PhysicsTestScene() override = default;

    void init() override;
    void handleEvent(sf::Event& event) override;
};
#endif
