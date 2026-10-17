//
// Created by MINEC on 2026/6/2.
//

#include <Scene.h>
#include "PhysicsWorld.h"
#include "ConfigManager.h"
#include "AssetManager.h"
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
    updateFpsStats(deltaTime);
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
    for (size_t i = 0; i < game_objects.size(); ++i) {
        if (const auto& obj = game_objects[i]; obj->isActive()) {
            if (obj->hasStarted()) obj->update(deltaTime);
            else obj->start();
        }
    }
}

void Scene::updateFpsStats(const eng::Time& deltaTime) {
    fps_accum += deltaTime.asSeconds();
    ++fps_frames;
    // 半秒结算一次：窗口太短数字乱跳，太长看不出瞬时掉帧
    if (fps_accum >= 0.5f) {
        fps_value = static_cast<float>(fps_frames) / fps_accum;
        fps_accum = 0.f;
        fps_frames = 0;
    }
}

#ifndef SERVER_BUILD
// 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除）。
// 基类默认 = 对象循环；场景级 override（Menu/Settings/SuperMario）
// 各自绘制自有内容并按需调用 renderObjects
void Scene::render(eng::Renderer& _renderer) {
    renderObjects(_renderer);
}

// 对象循环：遍历活跃对象调 render(eng::Renderer&)
void Scene::renderObjects(eng::Renderer& _renderer) {
    for (const auto& obj : game_objects) {
        if (obj->isActive()) {
            obj->render(_renderer);
        }
    }
}

void Scene::renderDebugHud(eng::Renderer& renderer) {
    if (!CONFIG.game.debug) return;   // 只在调试模式显示

    // 固定屏幕坐标：切默认视图，画完恢复原相机
    const eng::Renderer::CameraState oldCamera = renderer.getCamera();
    renderer.resetCamera();

    const eng::Vec2u win = renderer.getSize();
    const eng::FontHandle font = AssetManager::getInstance().getFontHandle();
    constexpr float font_size = 20.f;
    const int fps = static_cast<int>(fps_value + 0.5f);
    const std::string text = "FPS " + std::to_string(fps);
    const eng::Vec2f size = renderer.measureText(font, text, font_size);
    renderer.drawText(font, text,
                      eng::Vec2f(static_cast<float>(win.x) - size.x - 16.f, 12.f),
                      font_size,
                      fps < 50 ? eng::Color::Red
                      : fps < 100 ? eng::Color::Yellow
                                  : eng::Color::White);

    renderer.setCamera(oldCamera);
}

void Scene::handleEvent(const eng::EngineEvent& event) {
    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (size_t i = 0; i < game_objects.size(); ++i) {
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
    // 屏幕坐标 → 世界坐标：由渲染器统一换算（含相机缩放/平移，相机未激活时 1:1）；
    // 旧实现为纯平移，假设视口=窗口，固定视口缩放下会失准
    const eng::Vec2f world = renderer->screenToWorld(eng::Input::getMousePosition());
    return eng::Vec2i(static_cast<int>(world.x), static_cast<int>(world.y));
}
#endif
