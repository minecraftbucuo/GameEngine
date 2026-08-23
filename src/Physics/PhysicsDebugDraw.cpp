//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsDebugDraw.h"
#include "Core/Types.h"

#ifndef SERVER_BUILD

#include "PhysicsTypes.h"

#include <cmath>
#include <vector>

namespace physics {

eng::Color PhysicsDebugDraw::toColor(const b2Color& c) {
    return eng::Color(
        static_cast<eng::Uint8>(c.r * 255.0f),
        static_cast<eng::Uint8>(c.g * 255.0f),
        static_cast<eng::Uint8>(c.b * 255.0f),
        static_cast<eng::Uint8>(c.a * 255.0f)
    );
}

void PhysicsDebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
    if (!renderer || vertexCount < 2) return;

    // 闭合折线：首尾相连
    std::vector<eng::Vec2f> points;
    points.reserve(static_cast<std::size_t>(vertexCount) + 1);
    for (int32 i = 0; i < vertexCount; ++i) {
        points.push_back(toPixels(vertices[i]));
    }
    points.push_back(toPixels(vertices[0]));
    renderer->drawLines(points, toColor(color));
}

void PhysicsDebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
    if (!renderer || vertexCount < 3) return;

    // 半透明填充 + 同色描边（与迁移前 SFML 行为一致）
    std::vector<eng::Vec2f> points;
    points.reserve(static_cast<std::size_t>(vertexCount));
    for (int32 i = 0; i < vertexCount; ++i) {
        points.push_back(toPixels(vertices[i]));
    }
    eng::Color fill = toColor(color);
    fill.a = static_cast<eng::Uint8>(fill.a * 0.5f);
    renderer->drawPolygon(points, fill);
    points.push_back(points.front());
    renderer->drawLines(points, toColor(color));
}

void PhysicsDebugDraw::DrawCircle(const b2Vec2& center, float radius, const b2Color& color) {
    if (!renderer) return;

    renderer->drawCircle(toPixels(center), toPixels(radius), toColor(color), false);
}

void PhysicsDebugDraw::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) {
    if (!renderer) return;

    const float r = toPixels(radius);
    eng::Color fill = toColor(color);
    fill.a = static_cast<eng::Uint8>(fill.a * 0.5f);
    renderer->drawCircle(toPixels(center), r, fill, true, 1.f, toColor(color));

    // 朝向轴（半径方向的一根线）。axis 是单位方向向量，无量纲，不做米/像素换算
    const eng::Vec2f c = toPixels(center);
    renderer->drawLine(c, c + eng::Vec2f(axis.x, axis.y) * r, toColor(color));
}

void PhysicsDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) {
    if (!renderer) return;

    renderer->drawLine(toPixels(p1), toPixels(p2), toColor(color));
}

void PhysicsDebugDraw::DrawTransform(const b2Transform& xf) {
    if (!renderer) return;

    const float axisLen = 0.4f; // 米
    const eng::Vec2f p = toPixels(xf.p);

    // X 轴红色
    renderer->drawLine(p, p + toPixels(axisLen * xf.q.GetXAxis()), eng::Color::Red);
    // Y 轴绿色
    renderer->drawLine(p, p + toPixels(axisLen * xf.q.GetYAxis()), eng::Color::Green);
}

void PhysicsDebugDraw::DrawPoint(const b2Vec2& p, float size, const b2Color& color) {
    if (!renderer) return;

    renderer->drawCircle(toPixels(p), size, toColor(color), true);
}

void PhysicsDebugDraw::drawVelocities(b2World* world) {
    if (!renderer || !world) return;

    const float scale = 0.10f; // 速度(m/s) → 线长(像素) 的缩放，可按需调
    for (b2Body* body = world->GetBodyList(); body; body = body->GetNext()) {
        if (body->GetType() != b2_dynamicBody) continue; // 只画动态体
        b2Vec2 v = body->GetLinearVelocity();
        if (v.Length() < 0.05f) continue;                // 几乎静止的不画

        const eng::Vec2f p = toPixels(body->GetPosition());
        // 速度是 m/s，直接当米用 PPM 转成像素长度，再整体缩放
        const eng::Vec2f tip = p + toPixels(v) * scale;
        renderer->drawLine(p, tip, eng::Color::Cyan);

        // 箭头头部（两根短线）
        eng::Vec2f dir = tip - p;
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 8.0f) continue;
        dir /= len;
        const eng::Vec2f perp(-dir.y, dir.x);
        const eng::Vec2f headBase = tip - dir * 8.0f;
        renderer->drawLine(headBase + perp * 4.0f, tip, eng::Color::Cyan);
        renderer->drawLine(tip, headBase - perp * 4.0f, eng::Color::Cyan);
    }
}

} // namespace physics

#endif // SERVER_BUILD
