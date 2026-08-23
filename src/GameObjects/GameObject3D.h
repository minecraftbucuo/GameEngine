//
// Created by MINEC on 2026/3/25.
//

#pragma once
#include "Core/Types.h"
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

    void update(eng::Time deltaTime) override;

protected:
    void drawPoints(sf::RenderWindow* window) const {
        for (const auto& point : model->points) {
            drawPoint(window, trans(point, position));
        }
    }

    void drawFaces(sf::RenderWindow* window) const;

    static eng::Vec2f transToWindow(const eng::Vec2f& pos, eng::Vec2u windowSize);

    static eng::Vec2f project(const eng::Vec3f& pos) {
        return {pos.x / pos.z, pos.y / pos.z};
    }

    static void drawPoint(sf::RenderWindow* window, const eng::Vec2f& pos);

    static void drawEdge(sf::RenderWindow* window, const eng::Vec2f& p1, const eng::Vec2f& p2);

    static eng::Vec3f rotateXY(eng::Vec3f pos, float angle);

    static eng::Vec3f rotateXZ(eng::Vec3f pos, float angle);

    static eng::Vec3f rotateYZ(eng::Vec3f pos, float angle);

    eng::Vec3f rotate(const eng::Vec3f& pos) const {
        return rotateYZ(rotateXZ(rotateXY(pos, angleXY), angleXZ), angleYZ);
    }

    eng::Vec2f trans(const eng::Vec3f& point, const eng::Vec3f& pos) const {
        return transToWindow(project(rotate(point) + pos), getScene()->getWindowSize());
    }

    Model* model{};
    float angleXZ = 0.0f;
    float angleXY = 0.0f;
    float angleYZ = 0.0f;
    float sign = 1.0f;
    eng::Vec3f position;
};
#endif