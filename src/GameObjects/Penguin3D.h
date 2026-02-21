//
// Created by MINEC on 2026/1/3.
//

#pragma once

#include "GameObject.h"
#include "SceneContext.h"

class Penguin3D : public GameObject {
public:
    Penguin3D() {
        Model* p = ModelManager::getInstance().getModel("penguin");
        if (p == nullptr) {
            ModelManager::getInstance().loadModel("./Asset/penguin.obj", "penguin");
            this->model = ModelManager::getInstance().getModel("penguin");
        } else {
            this->model = p;
        }
        position = {0.0f, -0.5f, 1.0f};
        className = "Penguin3D";
    }

    void render(sf::RenderWindow* window) override {
        // drawPoints(window);
        drawFaces(window);
    }

    void update(const sf::Time deltaTime) override {
        angle += deltaTime.asSeconds();
    }

private:
    void drawPoints(sf::RenderWindow* window) const {
        for (const auto& point : model->points) {
            drawPoint(window, trans(point, position));
        }
    }

    void drawFaces(sf::RenderWindow* window) const {
        for (const auto& face : model->faces) {
            const int len = static_cast<int>(face.size());
            for (int i = 0; i < len; i++) {
                const int x = i, y = (i + 1) % len;
                const auto p1 = trans(model->points[face[x]], position);
                const auto p2 = trans(model->points[face[y]], position);
                drawEdge(window, p1, p2);
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

    sf::Vector2f trans(const sf::Vector3f& point, const sf::Vector3f& pos) const {
        return transform(project(rotateXZ(point, angle) + pos));
    }

    Model* model;
    float angle = 0.0f;
    sf::Vector3f position;
};