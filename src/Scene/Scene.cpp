//
// Created by MINEC on 2026/6/2.
//

#include <Scene.h>
#include "SceneContext.h"

void Scene::init() {
#ifndef SERVER_BUILD
    this->setCamera(window);
#endif
    this->setSceneContext();
    GameObject::resetIdCounter();
}

void Scene::update(sf::Time deltaTime) {
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
void Scene::render(sf::RenderWindow* _window) {
    for (const auto& obj : game_objects) {
        if (obj->isActive()) {
            obj->render(_window);
        }
    }
}

void Scene::handleEvent(sf::Event& event) {
    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        const auto& obj = game_objects[i];
        obj->handleEvent(event);
    }
    if (camera && event.type == sf::Event::Resized) {
        camera->resize();
    }
}
#endif

void Scene::addObjectWithMap(const std::shared_ptr<GameObject>& obj) {
    game_objects.push_back(obj);
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

void Scene::setSceneContext() const {
#ifndef SERVER_BUILD
    if (window) SceneContext::getInstance().setWindow(window);
    if (camera) SceneContext::getInstance().setCamera(camera.get());
#endif
    SceneContext::getInstance().setGameObjects(&game_objects);
    SceneContext::getInstance().setSceneManager(scene_manager);
}

void Scene::setSceneManager(SceneManager* _scene_manager) {
    scene_manager = _scene_manager;
    SceneContext::getInstance().setSceneManager(_scene_manager);
}
