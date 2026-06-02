//
// Created by MINEC on 2026/1/2.
//

#pragma once
#ifndef SERVER_BUILD
#include <SFML/Graphics.hpp>
#include <memory>
#include "Scene.h"

class CollisionSystem;
class GameObject;

class GameScene : public Scene {
public:
    explicit GameScene(sf::RenderWindow* _window) : Scene(_window, "GameScene") {}
    ~GameScene() override = default;

    void init() override;

    void initScene();

    void update(sf::Time deltaTime) override;

    void addObject(const std::shared_ptr<GameObject>& obj) override;

    void handleEvent(sf::Event& event) override;

    [[nodiscard]] CollisionSystem* getCollisionSystem() const override;

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
};
#endif
