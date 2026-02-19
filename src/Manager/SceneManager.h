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
    void handleEvent(sf::Event& event) const {
        currentScene->handleEvent(event);
    }

    void update(sf::Time deltaTime) const {
        currentScene->update(deltaTime);
    }

    void render(sf::RenderWindow* window) const {
        currentScene->render(window);
    }

    void loadScene(const std::string& scene_name) {
        if (scenes.find(scene_name) == scenes.end()) {
            std::cerr << "Scene " << scene_name << " not found" << std::endl;
            return;
        }
        currentScene = scenes[scene_name];
        currentScene->init();
    }

    template <typename T, typename... Args>
    void addScene(Args&&... args) {
        std::shared_ptr<T> scene = std::make_shared<T>(std::forward<Args>(args)...);
        scenes[scene->getSceneName()] = scene;
        scenes[scene->getSceneName()]->setSceneManager(this);
    }

    std::shared_ptr<Scene> getCurrentScene() const {
        return currentScene;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    std::shared_ptr<Scene> currentScene;
};
