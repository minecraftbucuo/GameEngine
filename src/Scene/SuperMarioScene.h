//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "Scene.h"
#include "CollisionSystem.h"
#include "NetworkManager.h"
#include "Core/Types.h"


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

    std::shared_ptr<GameObject> spawnEntityWithNetwork(sf::Packet& packet) override;

    void initStaticObjects();

    void initDynamicObjects();

#ifndef SERVER_BUILD
    void render(sf::RenderWindow* _window) override;
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

    NetworkManager::NetworkType getNetworkType() const override {
        return simple_network.getNetworkType();
    }

#ifndef SERVER_BUILD
    static void showDeathScreen(sf::RenderWindow* _window);
#endif

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
    NetworkManager simple_network;
#ifndef SERVER_BUILD
    sf::Sprite bg;
#endif
    bool is_initDynamicObjects = false;
    bool show_death_screen = false;
};
