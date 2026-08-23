//
// Created by MINEC on 2026/1/2.
//
#pragma once
#include <memory>
#include <vector>
#include <SFML/Graphics/RenderWindow.hpp>
#include "Core/Types.h"
#include "Core/Event.h"
#include "NetworkManager.h"
#include "Camera.h"

class GameObject;
class CollisionSystem;
class SceneManager;
namespace physics { class PhysicsWorld; }
class Scene {
public:
#ifndef SERVER_BUILD
    explicit Scene(sf::RenderWindow* _window) : window(_window), scene_name("Scene") {}
    explicit Scene(sf::RenderWindow* _window, std::string _name) : window(_window), scene_name(std::move(_name)) {}
#else
    explicit Scene() : scene_name("Scene") {}
    explicit Scene(std::string _name) : scene_name(std::move(_name)) {}
#endif
    virtual ~Scene();

    // 场景初始化方法
    virtual void init();

    virtual void exit() {

    }

    virtual std::shared_ptr<GameObject> spawnEntity() {
        return nullptr;
    }

    virtual std::shared_ptr<GameObject> spawnEntityWithNetwork() {
        return nullptr;
    }

    virtual std::shared_ptr<GameObject> spawnEntityWithNetwork(sf::Packet& packet) {
        return nullptr;
    }

    // 场景更新方法
    virtual void update(eng::Time deltaTime);

    // 场景渲染方法
#ifndef SERVER_BUILD
    virtual void render(sf::RenderWindow* _window);

    // 场景事件处理方法
    virtual void handleEvent(const eng::EngineEvent& event);
#endif

    // 游戏对象管理
    std::vector<std::shared_ptr<GameObject>>& getGameObjects() {
        return game_objects;
    }

    virtual void addObject(const std::shared_ptr<GameObject>& obj) {
        obj->setScene(this);
        game_objects.push_back(obj);
    }

    virtual void addObjectWithMap(const std::shared_ptr<GameObject>& obj);

    virtual void addObjectWithNetwork(const std::shared_ptr<GameObject>& obj) {
        addObjectWithMap(obj);
    }

    std::shared_ptr<GameObject> findGameObjectById(unsigned int id);

    void removeObjectById(unsigned int id);
#ifndef SERVER_BUILD
    // 相机管理
    void setCamera(sf::RenderWindow* _window) {
        camera = std::make_unique<Camera>(_window);
    }

    [[nodiscard]] Camera* getCamera() const {
        return camera.get();
    }
#endif

#ifndef SERVER_BUILD
    [[nodiscard]] eng::Vec2u getWindowSize() const {
        return window->getSize();
    }
#else
    static eng::Vec2u getWindowSize() {
        return {CONFIG.window.width, CONFIG.window.height};
    }
#endif

    [[nodiscard]] const std::string& getSceneName() const {
        return scene_name;
    }

    void setSceneManager(SceneManager* _scene_manager);

    [[nodiscard]] SceneManager* getSceneManager() const {
        return scene_manager;
    }

    [[nodiscard]] NetworkManager* getNetworkManager() const {
        return network_manager;
    }

    void setNetworkManager(NetworkManager* nm) {
        network_manager = nm;
    }

#ifndef SERVER_BUILD
    [[nodiscard]] sf::RenderWindow* getWindow() const {
        return window;
    }

    [[nodiscard]] eng::Vec2i getMousePosition() const;
#endif

    [[nodiscard]] virtual CollisionSystem* getCollisionSystem() const {
        return nullptr;
    }

    // ── Box2D 物理世界 ──
    // 子类构造时设 usePhysics = true 即启用物理，默认 false 不影响现有行为
    bool usePhysics = false;
    [[nodiscard]] physics::PhysicsWorld* getPhysicsWorld() const;

    virtual NetworkManager::NetworkType getNetworkType() const {
        return NetworkManager::NetworkType::None;
    }

protected:
    std::vector<std::shared_ptr<GameObject>> game_objects;
    std::unordered_map<unsigned int, std::shared_ptr<GameObject>> game_objects_map;

#ifndef SERVER_BUILD
    sf::RenderWindow* window{};
    std::unique_ptr<Camera> camera;
#endif

    std::string scene_name;
    SceneManager* scene_manager{};
    NetworkManager* network_manager{};
    bool is_init = false;

    // 物理世界（仅 usePhysics=true 时创建，析构在 Scene.cpp 处理）
    physics::PhysicsWorld* physics_world{};
};
