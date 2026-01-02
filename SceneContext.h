//
// Created by MINEC on 2025/12/10.
//

#pragma once
#include "Camera.h"
#include <SFML/Graphics.hpp>

class SceneContext {
public:
    static SceneContext& getInstance() {
        static SceneContext instance;
        return instance;
    }

    void setCamera(Camera* _camera) {
        this->camera = _camera;
    }

    void setWindow(sf::RenderWindow* _window) {
        this->window = _window;
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

private:
    sf::RenderWindow* window{};
    Camera* camera{};
};

