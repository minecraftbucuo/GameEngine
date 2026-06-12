//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD
#include "GameObject3D.h"
#include <cmath>

void GameObject3D::update(const sf::Time deltaTime) {
    angleXY += deltaTime.asSeconds();
    angleXZ += deltaTime.asSeconds();
    if (position.z > 10.f) sign = -1.0f;
    else if (position.z < 1.f) sign = 1.0f;
    position.z += 0.01f * sign;
}

void GameObject3D::drawFaces(sf::RenderWindow* window) const {
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

sf::Vector2f GameObject3D::transToWindow(const sf::Vector2f& pos, sf::Vector2u windowSize) {
    return {(pos.x + 1) / 2 * static_cast<float>(windowSize.x), (1 - pos.y) / 2 * static_cast<float>(windowSize.y)};
}

void GameObject3D::drawPoint(sf::RenderWindow* window, const sf::Vector2f& pos) {
    sf::CircleShape circle_shape;
    circle_shape.setRadius(4.0f);
    circle_shape.setOrigin(circle_shape.getRadius(), circle_shape.getRadius());
    circle_shape.setPosition(pos);
    window->draw(circle_shape);
}

void GameObject3D::drawEdge(sf::RenderWindow* window, const sf::Vector2f& p1, const sf::Vector2f& p2) {
    sf::VertexArray line(sf::Lines, 2);
    line[0].position = p1;
    line[1].position = p2;
    window->draw(line);
}

sf::Vector3f GameObject3D::rotateXY(sf::Vector3f pos, const float angle) {
    const float x = pos.x;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.x = pos.x * c - pos.y * s;
    pos.y = x * s + pos.y * c;
    return pos;
}

sf::Vector3f GameObject3D::rotateXZ(sf::Vector3f pos, const float angle) {
    const float x = pos.x;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.x = pos.x * c - pos.z * s;
    pos.z = x * s + pos.z * c;
    return pos;
}

sf::Vector3f GameObject3D::rotateYZ(sf::Vector3f pos, const float angle) {
    const float y = pos.y;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    pos.y = pos.y * c - pos.z * s;
    pos.z = y * s + pos.z * c;
    return pos;
}
#endif