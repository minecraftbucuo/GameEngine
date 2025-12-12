//
// Created by MINEC on 2025/12/10.
//

#pragma once

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

    [[nodiscard]] float getWorldWidth() const {
        return size.x;
    }

    [[nodiscard]] float getWorldHeight() const {
        return size.y;
    }

private:
    WorldContext() : size(0, 0) {}

private:
    sf::Vector2f size;
};

