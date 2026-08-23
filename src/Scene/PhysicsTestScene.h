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
    explicit PhysicsTestScene(eng::Renderer* _renderer) : Scene(_renderer, "PhysicsTestScene") {
        usePhysics = true;
    }
    ~PhysicsTestScene() override = default;

    void init() override;
    void handleEvent(const eng::EngineEvent& event) override;
    void exit() override;
    // SDL3 迁移 Step 6b：本场景渲染已全部走 Renderer 绘制命令（含 Box2D 调试绘制）
    void render(eng::Renderer& renderer) override;

    std::shared_ptr<PhysicsPlayer> player;
};
#endif
