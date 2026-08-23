//
// Created by MINEC on 2026/6/2.
//

#include "SceneManager.h"
#include "Core/Types.h"

void SceneManager::update(eng::Time deltaTime) const {
    currentScene->update(deltaTime);
}

void SceneManager::loadScene(const std::string& scene_name) {
    if (!scenes.contains(scene_name)) {
        LOG_ERROR_FMT("Scene {} not found", scene_name);
        return;
    }
    if (currentScene) currentScene->exit();
    currentScene = scenes[scene_name];
    currentScene->init();
}

std::shared_ptr<Scene> SceneManager::getCurrentScene() const {
    return currentScene;
}
