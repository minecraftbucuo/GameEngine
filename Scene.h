//
// Created by MINEC on 2026/1/2.
//
#pragma once
#include <memory>
#include <vector>
#include <SFML/Graphics.hpp>
#include "GameObject.h"
#include "SceneContext.h"
#include "Camera.h"

class Scene {
public:
    Scene(sf::RenderWindow* _window) : window(_window) {}
    virtual ~Scene() = default;

    // 场景初始化方法
    virtual void init() = 0;

    // 场景更新方法
    virtual void update(sf::Time deltaTime) {
        for (const auto& obj : game_objects) {
            if (obj->isActive()) {
                if (obj->hasStarted()) obj->update(deltaTime);
                else obj->start();
            }
        }
    }

    // 场景渲染方法
    virtual void render(sf::RenderWindow* _window) {
        for (const auto& obj : game_objects) {
            if (obj->isActive()) {
                obj->render(_window);
            }
        }
    }

    // 场景事件处理方法
    virtual void handleEvent(sf::Event& event) {
        if (camera) camera->handleEvent(event);
        for (const auto& obj : game_objects) {
            obj->handleEvent(event);
        }
        if (event.type == sf::Event::Resized) {
            camera->resize();
        }
    }

    // 游戏对象管理
    std::vector<std::shared_ptr<GameObject>>& getGameObjects() {
        return game_objects;
    }

    virtual void addObject(const std::shared_ptr<GameObject>& obj) {
        game_objects.push_back(obj);
    }

    // 相机管理
    void setCamera(sf::RenderWindow* _window) {
        camera = std::make_unique<Camera>(_window);
    }

    [[nodiscard]] Camera* getCamera() const {
        return camera.get();
    }

    // 场景大小和上下文管理
    void setSceneContext() const {
        if (camera) SceneContext::getInstance().setCamera(camera.get());
        if (window) SceneContext::getInstance().setWindow(window);
    }

    [[nodiscard]] sf::Vector2u getWindowSize() const {
        return window->getSize();
    }

protected:
    std::vector<std::shared_ptr<GameObject>> game_objects;
    sf::RenderWindow* window{};
    std::unique_ptr<Camera> camera;
};
