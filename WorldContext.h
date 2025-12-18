//
// Created by MINEC on 2025/12/10.
//

#pragma once
#include "Camera.h"

class WorldContext {
public:
    static WorldContext& getInstance() {
        static WorldContext instance;
        return instance;
    }

    void setWorldSize(const float width, const float height) {
        size.x = width;
        size.y = height;
    }

    void setCamera(Camera* camera) {
        this->camera = camera;
    }

    [[nodiscard]] Camera* getCamera() const {
        return camera;
    }

    [[nodiscard]] float getWorldWidth() const {
        return size.x;
    }

    [[nodiscard]] float getWorldHeight() const {
        return size.y;
    }

private:
    WorldContext() : size(0, 0) {}
    sf::Vector2f size;
    Camera* camera{};
};

