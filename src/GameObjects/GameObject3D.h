//
// Created by MINEC on 2026/3/25.
//

#pragma once
#ifndef SERVER_BUILD
#include "GameObject.h"
#include "Scene.h"
#include "ModelManager.h"

class GameObject3D : public GameObject {
public:
    GameObject3D() = default;

    void render(sf::RenderWindow* window) override {
        drawPoints(window);
        drawFaces(window);
    }

    void update(sf::Time deltaTime) override;

protected:
    void drawPoints(sf::RenderWindow* window) const {
        for (const auto& point : model->points) {
            drawPoint(window, trans(point, position));
        }
    }

    void drawFaces(sf::RenderWindow* window) const;

    static sf::Vector2f transToWindow(const sf::Vector2f& pos, sf::Vector2u windowSize);

    static sf::Vector2f project(const sf::Vector3f& pos) {
        return {pos.x / pos.z, pos.y / pos.z};
    }

    static void drawPoint(sf::RenderWindow* window, const sf::Vector2f& pos);

    static void drawEdge(sf::RenderWindow* window, const sf::Vector2f& p1, const sf::Vector2f& p2);

    static sf::Vector3f rotateXY(sf::Vector3f pos, float angle);

    static sf::Vector3f rotateXZ(sf::Vector3f pos, float angle);

    static sf::Vector3f rotateYZ(sf::Vector3f pos, float angle);

    sf::Vector3f rotate(const sf::Vector3f& pos) const {
        return rotateYZ(rotateXZ(rotateXY(pos, angleXY), angleXZ), angleYZ);
    }

    sf::Vector2f trans(const sf::Vector3f& point, const sf::Vector3f& pos) const {
        return transToWindow(project(rotate(point) + pos), getScene()->getWindowSize());
    }

    Model* model{};
    float angleXZ = 0.0f;
    float angleXY = 0.0f;
    float angleYZ = 0.0f;
    float sign = 1.0f;
    sf::Vector3f position;
};
#endif