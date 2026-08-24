//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "Scene.h"
#include "CollisionSystem.h"
#include "NetworkManager.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Handles.h"
#endif


class SuperMarioScene : public Scene {
public:
#ifndef SERVER_BUILD
    explicit SuperMarioScene(eng::Renderer* _renderer) : Scene(_renderer, "SuperMarioScene") {
    }
#else
    explicit SuperMarioScene() : Scene("SuperMarioScene") { }
#endif

    ~SuperMarioScene() override = default;

    void init() override;

    void exit() override;

    std::shared_ptr<GameObject> spawnEntity() override;

    std::shared_ptr<GameObject> spawnEntityWithNetwork() override;

    std::shared_ptr<GameObject> spawnEntityWithNetwork(eng::Packet& packet) override;

    void initStaticObjects();

    void initDynamicObjects();

#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

    void update(eng::Time deltaTime) override;

    void addObject(const std::shared_ptr<GameObject>& obj) override;

    void addObjectWithNetwork(const std::shared_ptr<GameObject>& obj) override;
#ifndef SERVER_BUILD
    void handleEvent(const eng::EngineEvent& event) override;
#endif
    CollisionSystem* getCollisionSystem() const override {
        return collisionSystem.get();
    }

    void startServer();

    void connectToServer(const std::string& address);

    // N4 修复：重进场景时清上一局会话——Scene::exit 为空且场景实例常驻缓存，
    // 不重置则上一局的马里奥/网络状态跨局残留（Client→单机出现双马里奥的根因）
    void resetSession();

    NetworkManager::NetworkType getNetworkType() const override {
        return simple_network.getNetworkType();
    }

#ifndef SERVER_BUILD
    static void showDeathScreen(eng::Renderer& renderer);

    // N4 断线反馈：与死亡屏同构的提示层（半透明遮罩 + ESC 回菜单），桌面/WEB 共用
    static void showDisconnectScreen(eng::Renderer& renderer);
#endif

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
    NetworkManager simple_network;
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：背景数据化（原 sf::Sprite），dst 在 init 按窗口高度等比算出
    eng::TextureHandle bg_texture;
    eng::FloatRect bg_dst;
#endif
    bool is_initDynamicObjects = false;
    bool show_death_screen = false;
    bool show_disconnect_screen = false;   // N4：simple_network 断线标志驱动，重进场景复位
};
