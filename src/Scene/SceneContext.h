//
// Created by MINEC on 2025/12/10.
//

#pragma once
#include "Camera.h"
#include "GameObject.h"
#include <SFML/Graphics.hpp>

class SceneContext {
public:
    static SceneContext& getInstance() {
        static SceneContext instance;
        return instance;
    }

    void setCamera(Camera* _camera) {
        this->camera = _camera;
        this->camera->init();
    }

    void setWindow(sf::RenderWindow* _window) {
        this->window = _window;
    }

    void setGameObjects(const std::vector<std::shared_ptr<GameObject>>* _game_objects) {
        game_objects = _game_objects;
    }

    [[nodiscard]] Camera* getCamera() const {
        return camera;
    }

    [[nodiscard]] unsigned int getWindowWidth() const {
        if (window) return window->getSize().x;
        return 0;
    }

    [[nodiscard]] unsigned int getWindowHeight() const {
        if (window) return window->getSize().y;
        return 0;
    }

    [[nodiscard]] sf::RenderWindow* getWindow() const {
        return window;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<GameObject>>* getGameObjects() const {
        return game_objects;
    }

private:
    sf::RenderWindow* window{};
    Camera* camera{};
    const std::vector<std::shared_ptr<GameObject>>* game_objects{};
};

