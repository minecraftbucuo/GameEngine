//
// Created by MINEC on 2026/5/8.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "GameObject3D.h"
#include "Render/Renderer.h"
#include <cmath>

void GameObject3D::update(const eng::Time deltaTime) {
    angleXY += deltaTime.asSeconds();
    angleXZ += deltaTime.asSeconds();
    if (position.z > 10.f) sign = -1.0f;
    else if (position.z < 1.f) sign = 1.0f;
    position.z += 0.01f * sign;
}

void GameObject3D::drawFaces(eng::Renderer& renderer) const {
    for (const auto& face : model->faces) {
        const int len = static_cast<int>(face.size());
        for (int i = 0; i < len; i++) {
            const int x = i, y = (i + 1) % len;
            const auto p1 = trans(model->points[face[x]], position);
            const auto p2 = trans(model->points[face[y]], position);
            drawEdge(renderer, p1, p2);
        }
    }
}

eng::Vec2f GameObject3D::transToWindow(const eng::Vec2f& pos, eng::Vec2u windowSize) {
    return {(pos.x + 1) / 2 * static_cast<float>(windowSize.x), (1 - pos.y) / 2 * static_cast<float>(windowSize.y)};
}

// 顶点 = 4px 白色圆（原 setOrigin(radius,radius) ⇒ pos 为圆心）
void GameObject3D::drawPoint(eng::Renderer& renderer, const eng::Vec2f& pos) {
    renderer.drawCircle(pos, 4.f, eng::Color::White);
}

// 边 = 白色线段
void GameObject3D::drawEdge(eng::Renderer& renderer, const eng::Vec2f& p1, const eng::Vec2f& p2) {
    renderer.drawLine(p1, p2, eng::Color::White);
}

eng::Vec3f GameObject3D::rotateXY(eng::Vec3f pos, const float angle) {
    const float x = pos.x;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.x = pos.x * c - pos.y * s;
    pos.y = x * s + pos.y * c;
    return pos;
}

eng::Vec3f GameObject3D::rotateXZ(eng::Vec3f pos, const float angle) {
    const float x = pos.x;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.x = pos.x * c - pos.z * s;
    pos.z = x * s + pos.z * c;
    return pos;
}

eng::Vec3f GameObject3D::rotateYZ(eng::Vec3f pos, const float angle) {
    const float y = pos.y;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.y = pos.y * c - pos.z * s;
    pos.z = y * s + pos.z * c;
    return pos;
}
#endif