//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsDebugDraw.h"

#ifndef SERVER_BUILD

#include "PhysicsTypes.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <cmath>

namespace physics {

sf::Color PhysicsDebugDraw::toSfColor(const b2Color& c) {
    return sf::Color(
        static_cast<sf::Uint8>(c.r * 255.0f),
        static_cast<sf::Uint8>(c.g * 255.0f),
        static_cast<sf::Uint8>(c.b * 255.0f),
        static_cast<sf::Uint8>(c.a * 255.0f)
    );
}

void PhysicsDebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
    if (!window || vertexCount < 2) return;

    sf::VertexArray lines(sf::LineStrip, static_cast<size_t>(vertexCount) + 1);
    for (int32 i = 0; i < vertexCount; ++i) {
        sf::Vector2f p = toPixels(vertices[i]);
        lines[static_cast<size_t>(i)].position = p;
        lines[static_cast<size_t>(i)].color = toSfColor(color);
    }
    // 闭合
    lines[static_cast<size_t>(vertexCount)].position = toPixels(vertices[0]);
    lines[static_cast<size_t>(vertexCount)].color = toSfColor(color);
    window->draw(lines);
}

void PhysicsDebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
    if (!window || vertexCount < 3) return;

    sf::ConvexShape shape(static_cast<size_t>(vertexCount));
    sf::Color fill = toSfColor(color);
    fill.a = static_cast<sf::Uint8>(fill.a * 0.5f); // 半透明填充
    for (int32 i = 0; i < vertexCount; ++i) {
        shape.setPoint(static_cast<size_t>(i), toPixels(vertices[i]));
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(toSfColor(color));
    shape.setOutlineThickness(1.0f);
    window->draw(shape);
}

void PhysicsDebugDraw::DrawCircle(const b2Vec2& center, float radius, const b2Color& color) {
    if (!window) return;

    float r = toPixels(radius);
    sf::CircleShape shape(r);
    shape.setOrigin(r, r);
    shape.setPosition(toPixels(center));
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(toSfColor(color));
    shape.setOutlineThickness(1.0f);
    window->draw(shape);
}

void PhysicsDebugDraw::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) {
    if (!window) return;

    float r = toPixels(radius);
    sf::CircleShape shape(r);
    shape.setOrigin(r, r);
    shape.setPosition(toPixels(center));
    sf::Color fill = toSfColor(color);
    fill.a = static_cast<sf::Uint8>(fill.a * 0.5f);
    shape.setFillColor(fill);
    shape.setOutlineColor(toSfColor(color));
    shape.setOutlineThickness(1.0f);
    window->draw(shape);

    // 朝向轴（半径方向的一根线）。axis 是单位方向向量，无量纲，不做米/像素换算
    sf::Vector2f c = toPixels(center);
    sf::Vector2f dir(axis.x, axis.y);
    sf::Vertex line[] = {
        sf::Vertex(c, toSfColor(color)),
        sf::Vertex(c + dir * r, toSfColor(color))
    };
    window->draw(line, 2, sf::Lines);
}

void PhysicsDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) {
    if (!window) return;

    sf::Vertex line[] = {
        sf::Vertex(toPixels(p1), toSfColor(color)),
        sf::Vertex(toPixels(p2), toSfColor(color))
    };
    window->draw(line, 2, sf::Lines);
}

void PhysicsDebugDraw::DrawTransform(const b2Transform& xf) {
    if (!window) return;

    const float axisLen = 0.4f; // 米
    sf::Vector2f p = toPixels(xf.p);

    // X 轴红色
    sf::Vertex xAxis[] = {
        sf::Vertex(p, sf::Color::Red),
        sf::Vertex(p + toPixels(axisLen * xf.q.GetXAxis()), sf::Color::Red)
    };
    window->draw(xAxis, 2, sf::Lines);

    // Y 轴绿色
    sf::Vertex yAxis[] = {
        sf::Vertex(p, sf::Color::Green),
        sf::Vertex(p + toPixels(axisLen * xf.q.GetYAxis()), sf::Color::Green)
    };
    window->draw(yAxis, 2, sf::Lines);
}

void PhysicsDebugDraw::DrawPoint(const b2Vec2& p, float size, const b2Color& color) {
    if (!window) return;

    sf::CircleShape shape(size);
    shape.setOrigin(size, size);
    shape.setPosition(toPixels(p));
    shape.setFillColor(toSfColor(color));
    window->draw(shape);
}

void PhysicsDebugDraw::drawVelocities(b2World* world) {
    if (!window || !world) return;

    const float scale = 0.10f; // 速度(m/s) → 线长(像素) 的缩放，可按需调
    for (b2Body* body = world->GetBodyList(); body; body = body->GetNext()) {
        if (body->GetType() != b2_dynamicBody) continue; // 只画动态体
        b2Vec2 v = body->GetLinearVelocity();
        if (v.Length() < 0.05f) continue;                // 几乎静止的不画

        sf::Vector2f p = toPixels(body->GetPosition());
        // 速度是 m/s，直接当米用 PPM 转成像素长度，再整体缩放
        sf::Vector2f tip = p + toPixels(v) * scale;
        sf::Vertex line[] = {
            sf::Vertex(p, sf::Color::Cyan),
            sf::Vertex(tip, sf::Color::Cyan)
        };
        window->draw(line, 2, sf::Lines);

        // 箭头头部（两根短线）
        sf::Vector2f dir = tip - p;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 8.0f) continue;
        dir /= len;
        sf::Vector2f perp(-dir.y, dir.x);
        sf::Vector2f headBase = tip - dir * 8.0f;
        sf::Vertex head[] = {
            sf::Vertex(headBase + perp * 4.0f, sf::Color::Cyan),
            sf::Vertex(tip, sf::Color::Cyan),
            sf::Vertex(tip, sf::Color::Cyan),
            sf::Vertex(headBase - perp * 4.0f, sf::Color::Cyan)
        };
        window->draw(head, 4, sf::Lines);
    }
}

} // namespace physics

#endif // SERVER_BUILD
