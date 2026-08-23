//
// Created by MINEC on 2026/6/2.
//

#include <Scene.h>
#include "PhysicsWorld.h"
#include "ConfigManager.h"
#include "Core/Types.h"
#include "Core/Input.h"

// 析构定义放这里，确保 PhysicsWorld 完整类型可见
Scene::~Scene() {
    delete physics_world;
    physics_world = nullptr;
}

void Scene::init() {
#ifndef SERVER_BUILD
    this->setCamera(renderer);
#endif
    GameObject::resetIdCounter();
    // 仅当场景启用物理时创建物理世界
    if (usePhysics && !physics_world) {
        physics_world = new physics::PhysicsWorld();
    }
}

void Scene::update(eng::Time deltaTime) {
    // 物理世界推进（在对象 update 前，保证 PhysicsBodyComponent 同步最新位置）
    if (physics_world) {
        physics_world->step(deltaTime);
    }
    // 删除已销毁的 GameObject
    std::erase_if(game_objects, [](const auto& obj) {
        return obj->isDestroy();
    });
    for (auto it = game_objects_map.begin(); it != game_objects_map.end();) {
        if (it->second->isDestroy()) {
            it = game_objects_map.erase(it);
        }
        else {
            ++it;
        }
    }
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        if (const auto& obj = game_objects[i]; obj->isActive()) {
            if (obj->hasStarted()) obj->update(deltaTime);
            else obj->start();
        }
    }
}

#ifndef SERVER_BUILD
// 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除）。
// 基类默认 = 对象循环；场景级 override（Menu/Settings/SuperMario/PhysicsTest）
// 各自绘制自有内容并按需调用 renderObjects
void Scene::render(eng::Renderer& _renderer) {
    renderObjects(_renderer);
    // Box2D 调试绘制已随 SDL3 迁移 6b 移入 PhysicsTestScene::render(eng::Renderer&)
}

// 对象循环：遍历活跃对象调 render(eng::Renderer&)
void Scene::renderObjects(eng::Renderer& _renderer) {
    for (const auto& obj : game_objects) {
        if (obj->isActive()) {
            obj->render(_renderer);
        }
    }
}

void Scene::handleEvent(const eng::EngineEvent& event) {
    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        const auto& obj = game_objects[i];
        obj->handleEvent(event);
    }
    if (camera && event.type == eng::EventType::WindowResize) {
        camera->resize();
    }
}
#endif

void Scene::addObjectWithMap(const std::shared_ptr<GameObject>& obj) {
    addObject(obj);
    game_objects_map[obj->getId()] = obj;
}

std::shared_ptr<GameObject> Scene::findGameObjectById(const unsigned int id) {
    if (!game_objects_map.contains(id)) {
        LOG_ERROR_FMT("GameObject with ID {} are not found in game_objects_map", id);
        return nullptr;
    }
    return game_objects_map[id];
}

void Scene::removeObjectById(const unsigned int id) {
    if (!game_objects_map.contains(id)) {
        LOG_ERROR_FMT("GameObject with ID {} are not found in game_objects_map", id);
        return;
    }
    game_objects_map.erase(id);
    LOG_DEBUG_FMT("Removing GameObject with id {} in game_objects_map", id);
    for (auto it = game_objects.begin(); it != game_objects.end(); ++it) {
        if ((*it)->getId() == id) {
            game_objects.erase(it);
            LOG_DEBUG_FMT("Removing GameObject with id {} in game_objects", id);
            break;
        }
    }
}

void Scene::setSceneManager(SceneManager* _scene_manager) {
    scene_manager = _scene_manager;
}

physics::PhysicsWorld* Scene::getPhysicsWorld() const {
    return physics_world;
}

#ifndef SERVER_BUILD
eng::Vec2i Scene::getMousePosition() const {
    if (!renderer || !camera) return {};
    const eng::Vec2f camera_center = camera->getCenter();
    const eng::Vec2u window_size = renderer->getSize();
    eng::Vec2i mouse_position = eng::Input::getMousePosition();
    mouse_position.x += static_cast<int>(camera_center.x - window_size.x * 0.5f);
    mouse_position.y += static_cast<int>(camera_center.y - window_size.y * 0.5f);
    return mouse_position;
}
#endif
