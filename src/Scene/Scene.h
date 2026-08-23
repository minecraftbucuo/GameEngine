//
// Created by MINEC on 2026/1/2.
//
#pragma once
#include <memory>
#include <vector>
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
    explicit Scene(eng::Renderer* _renderer)
        : renderer(_renderer), scene_name("Scene") {}
    explicit Scene(eng::Renderer* _renderer, std::string _name)
        : renderer(_renderer), scene_name(std::move(_name)) {}
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

    virtual std::shared_ptr<GameObject> spawnEntityWithNetwork(eng::Packet& packet) {
        return nullptr;
    }

    // 场景更新方法
    virtual void update(eng::Time deltaTime);

    // 场景渲染方法
#ifndef SERVER_BUILD
    // 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除，唯一虚签名；
    // 基类默认 = renderObjects，场景级 override 自行决定是否调用它）
    virtual void render(eng::Renderer& _renderer);

    // 对象循环：遍历活跃对象调 render(eng::Renderer&)
    void renderObjects(eng::Renderer& _renderer);

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
    void setCamera(eng::Renderer* _renderer) {
        camera = std::make_unique<Camera>(_renderer);
    }

    [[nodiscard]] Camera* getCamera() const {
        return camera.get();
    }

    [[nodiscard]] eng::Renderer* getRenderer() const {
        return renderer;
    }
#endif

#ifndef SERVER_BUILD
    [[nodiscard]] eng::Vec2u getWindowSize() const {
        return renderer->getSize();
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
    eng::Renderer* renderer{};
    std::unique_ptr<Camera> camera;
#endif

    std::string scene_name;
    SceneManager* scene_manager{};
    NetworkManager* network_manager{};
    bool is_init = false;

    // 物理世界（仅 usePhysics=true 时创建，析构在 Scene.cpp 处理）
    physics::PhysicsWorld* physics_world{};
};
