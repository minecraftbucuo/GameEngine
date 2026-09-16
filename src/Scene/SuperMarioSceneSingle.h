//
// Created by MINEC on 2026/9/16.
//
// 马里奥单机场景（客户端专属）：本地权威、零网络同步。
// 地形与联机版 SuperMarioSceneMultiplayer 一致；敌人（Goomba）为单机版专属
// ——联机敌人同步未实现，联机版不生成；
// 不持有 NetworkManager，Mario/FireBall 经 getNetworkManager()==nullptr 自动走本地分支。

#pragma once
#ifndef SERVER_BUILD
#include "Scene.h"
#include "CollisionSystem.h"
#include "Core/Types.h"
#include "Render/Handles.h"


class SuperMarioSceneSingle : public Scene {
public:
    explicit SuperMarioSceneSingle(eng::Renderer* _renderer)
        : Scene(_renderer, "SuperMarioSceneSingle") {
    }

    ~SuperMarioSceneSingle() override = default;

    void init() override;

    void exit() override;

    void initStaticObjects();

    void initDynamicObjects();

    void render(eng::Renderer& renderer) override;

    void update(eng::Time deltaTime) override;

    void addObject(const std::shared_ptr<GameObject>& obj) override;

    void handleEvent(const eng::EngineEvent& event) override;

    CollisionSystem* getCollisionSystem() const override {
        return collisionSystem.get();
    }

    // N4 修复：重进场景时清上一局会话——Scene::exit 为空且场景实例常驻缓存，
    // 不重置则上一局的马里奥跨局残留（重进场景出现双马里奥的根因）
    void resetSession();

    static void showDeathScreen(eng::Renderer& renderer);

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
    // SDL3 迁移 6c：背景数据化（原 sf::Sprite），dst 在 init 按窗口高度等比算出
    eng::TextureHandle bg_texture;
    eng::FloatRect bg_dst;
    bool is_initDynamicObjects = false;
    bool show_death_screen = false;
};
#endif
