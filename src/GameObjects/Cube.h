//
// Created by MINEC on 2026/1/3.
//

#pragma once

#include "GameObject.h"
#include "SceneContext.h"

class Cube : public GameObject {
public:
    void render(sf::RenderWindow* window) override {
        for (const auto& point : points) {
            drawPoint(window, transform(project(addZ(rotateXZ(point, angle), z))));
        }
        for (const auto& edge : edges) {
            const auto p1 = project(addZ(rotateXZ(points[edge.first], angle), z));
            const auto p2 = project(addZ(rotateXZ(points[edge.second], angle), z));
            drawEdge(window, transform(p1), transform(p2));
        }
    }

    void update(sf::Time deltaTime) override {
        for (auto& point : points) {
            point = rotateXY(point, deltaTime.asSeconds());
        }
        angle += deltaTime.asSeconds();
        if (z > 10.f) sign = -1.0f;
        else if (z < 1.f) sign = 1.0f;
        z += 0.01f * sign;
    }

private:
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

    std::array<sf::Vector3f, 8> points = {
        sf::Vector3f(-0.5f, 0.5f, -0.5f),
        sf::Vector3f(0.5f, 0.5f, -0.5f),
        sf::Vector3f(0.5f, -0.5f, -0.5f),
        sf::Vector3f(-0.5f, -0.5f, -0.5f),
        sf::Vector3f(-0.5f, 0.5f, 0.5f),
        sf::Vector3f(0.5f, 0.5f, 0.5f),
        sf::Vector3f(0.5f, -0.5f, 0.5f),
        sf::Vector3f(-0.5f, -0.5f, 0.5f)
    };
    std::array<std::pair<int, int>, 12> edges = {
        std::make_pair(0, 1),
        std::make_pair(0, 4),
        std::make_pair(0, 3),
        std::make_pair(1, 2),
        std::make_pair(1, 5),
        std::make_pair(2, 3),
        std::make_pair(2, 6),
        std::make_pair(3, 7),
        std::make_pair(4, 5),
        std::make_pair(4, 7),
        std::make_pair(5, 6),
        std::make_pair(6, 7)
    };
    sf::Vector3f position;
    float z = 3.0f;
    float angle = 0.0f;
    float sign = 1.0f;
};