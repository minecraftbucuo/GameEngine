//
// Created by MINEC on 2026/8/18.
//

#pragma once
#ifndef SERVER_BUILD
#include "Scene.h"
#include "EventBus.h"
#include "Events.h"

class PhysicsPlayer;

class PhysicsTestScene : public Scene {
public:
    explicit PhysicsTestScene(sf::RenderWindow* _window) : Scene(_window, "PhysicsTestScene") {
        usePhysics = true;
    }
    ~PhysicsTestScene() override = default;

    void init() override;
    void handleEvent(sf::Event& event) override;
    void exit() override;

    std::shared_ptr<PhysicsPlayer> player;
};
#endif
