//
// Created by MINEC on 2026/6/2.
//

#include "SceneContext.h"
#include "ConfigManager.h"

SceneContext& SceneContext::getInstance() {
    static SceneContext instance;
    return instance;
}

#ifndef SERVER_BUILD
void SceneContext::setCamera(Camera* _camera) {
    this->camera = _camera;
    this->camera->init();
}

void SceneContext::setWindow(sf::RenderWindow* _window) {
    this->window = _window;
}
#endif

void SceneContext::setGameObjects(const std::vector<std::shared_ptr<GameObject>>* _game_objects) {
    game_objects = _game_objects;
}

void SceneContext::setSceneManager(SceneManager* _scene_manager) {
    scene_manager = _scene_manager;
}

#ifndef SERVER_BUILD
Camera* SceneContext::getCamera() const {
    return camera;
}

sf::RenderWindow* SceneContext::getWindow() const {
    return window;
}
#endif

unsigned int SceneContext::getWindowWidth() const {
#ifndef SERVER_BUILD
    if (window) return window->getSize().x;
#endif
    return CONFIG.window.width;
}

unsigned int SceneContext::getWindowHeight() const {
#ifndef SERVER_BUILD
    if (window) return window->getSize().y;
#endif
    return CONFIG.window.height;
}

const std::vector<std::shared_ptr<GameObject>>* SceneContext::getGameObjects() const {
    return game_objects;
}

SceneManager* SceneContext::getSceneManager() const {
    return scene_manager;
}

#ifndef SERVER_BUILD
sf::Vector2i SceneContext::getMousePosition() const {
    const sf::Vector2f camera_center = camera->getCenter();
    const sf::Vector2u window_size = window->getSize();
    sf::Vector2i mouse_position = sf::Mouse::getPosition(*window);
    mouse_position.x += camera_center.x - window_size.x * 0.5f;
    mouse_position.y += camera_center.y - window_size.y * 0.5f;
    return mouse_position;
}
#endif

