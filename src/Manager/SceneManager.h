//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Scene.h"
#include "Core/Types.h"
#include "Core/Event.h"

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;
#ifndef SERVER_BUILD
    void handleEvent(const eng::EngineEvent& event) const {
        currentScene->handleEvent(event);
    }
#endif

    void update(eng::Time deltaTime) const;
#ifndef SERVER_BUILD
    // SDL3 迁移 Step 6a：render 分发切换到 Renderer（唯一调用点 GameEngine 主循环已同步）
    void render(eng::Renderer& renderer) const {
        currentScene->render(renderer);
    }
#endif

    void loadScene(const std::string& scene_name);

    template <typename T, typename... Args>
    void addScene(Args&&... args) {
        std::shared_ptr<T> scene = std::make_shared<T>(std::forward<Args>(args)...);
        scenes[scene->getSceneName()] = scene;
        scenes[scene->getSceneName()]->setSceneManager(this);
    }

    std::shared_ptr<Scene> getCurrentScene() const;

private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    std::shared_ptr<Scene> currentScene;
};
