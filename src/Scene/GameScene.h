//
// Created by MINEC on 2026/1/2.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <memory>
#include "Scene.h"

class CollisionSystem;
class GameObject;

class GameScene : public Scene {
public:
    explicit GameScene(eng::Renderer* _renderer) : Scene(_renderer, "GameScene") {}
    ~GameScene() override = default;

    void init() override;

    void initScene();

    void update(eng::Time deltaTime) override;

    void addObject(const std::shared_ptr<GameObject>& obj) override;

    void handleEvent(const eng::EngineEvent& event) override;

    [[nodiscard]] CollisionSystem* getCollisionSystem() const override;

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
};
#endif
