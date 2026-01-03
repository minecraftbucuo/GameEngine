//
// Created by MINEC on 2026/1/3.
//

#pragma once

#include "GameObject.h"
#include "SceneContext.h"

class Cube : public GameObject {
public:
    void render(sf::RenderWindow* window) override {
        drawPoints(window);
        drawFaces(window);
    }

    void update(const sf::Time deltaTime) override {
        for (auto& point : points) {
            point = rotateXY(point, deltaTime.asSeconds());
        }
        angle += deltaTime.asSeconds();
        if (z > 10.f) sign = -1.0f;
        else if (z < 1.f) sign = 1.0f;
        z += 0.01f * sign;
    }

private:
    void drawPoints(sf::RenderWindow* window) const {
        for (const auto& point : points) {
            drawPoint(window, transform(project(addZ(rotateXZ(point, angle), z))));
        }
    }

    void drawFaces(sf::RenderWindow* window) const {
        for (const auto& face : faces) {
            const int len = static_cast<int>(face.size());
            for (int i = 0; i < len; i++) {
                const int x = i, y = (i + 1) % len;
                const auto p1 = project(addZ(rotateXZ(points[face[x]], angle), z));
                const auto p2 = project(addZ(rotateXZ(points[face[y]], angle), z));
                drawEdge(window, transform(p1), transform(p2));
            }
        }
    }

    static sf::Vector2f transform(const sf::Vector2f& pos) {
        const auto scene_context = SceneContext::getInstance();
        const unsigned width = scene_context.getWindowWidth();
        const unsigned height = scene_context.getWindowHeight();
        return {(pos.x + 1) / 2 * static_cast<float>(width), (1 - pos.y) / 2 * static_cast<float>(height)};
    }

    static sf::Vector2f project(const sf::Vector3f& pos) {
        return {pos.x / pos.z, pos.y / pos.z};
    }

    static void drawPoint(sf::RenderWindow* window, const sf::Vector2f& pos) {
        sf::CircleShape circle_shape;
        circle_shape.setRadius(4.0f);
        circle_shape.setOrigin(circle_shape.getRadius(), circle_shape.getRadius());
        circle_shape.setPosition(pos);
        window->draw(circle_shape);
    }

    static void drawEdge(sf::RenderWindow* window, const sf::Vector2f& p1, const sf::Vector2f& p2) {
        sf::VertexArray line(sf::Lines, 2);
        line[0].position = p1;
        line[1].position = p2;
        window->draw(line);
    }

    static sf::Vector3f rotateXY(sf::Vector3f pos, const float angle) {
        const float x = pos.x;
        const float s = std::sin(angle);
        const float c = std::cos(angle);
        pos.x = pos.x * c - pos.y * s;
        pos.y = x * s + pos.y * c;
        return pos;
    }

    static sf::Vector3f rotateXZ(sf::Vector3f pos, const float angle) {
        const float x = pos.x;
        const float s = std::sin(angle);
        const float c = std::cos(angle);
        pos.x = pos.x * c - pos.z * s;
        pos.z = x * s + pos.z * c;
        return pos;
    }

    static sf::Vector3f addZ(const sf::Vector3f& pos, const float dz) {
        return {pos.x, pos.y, pos.z + dz};
    }

    std::vector<sf::Vector3f> points = {
        {-0.5f, 0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f}
    };
    std::vector<std::vector<int>> faces = {
        {0, 1, 2 ,3},
        {1, 5, 6, 2},
        {4, 5, 6, 7},
        {0, 3, 7, 4}
    };
    float z = 3.0f;
    float angle = 0.0f;
    float sign = 1.0f;
};