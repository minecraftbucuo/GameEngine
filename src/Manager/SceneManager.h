//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Scene.h"

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;
#ifndef SERVER_BUILD
    void handleEvent(sf::Event& event) const {
        currentScene->handleEvent(event);
    }
#endif

    void update(sf::Time deltaTime) const;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) const {
        currentScene->render(window);
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
