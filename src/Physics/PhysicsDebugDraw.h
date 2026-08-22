//
// Created by MINEC on 2026/8/18.
//

#pragma once

#ifndef SERVER_BUILD

#include <box2d/box2d.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Color.hpp>

namespace physics {

// Box2D 调试绘制：把 b2Draw 的调用翻译成 SFML 绘制
// 坐标转换：Box2D 米 → SFML 像素（PhysicsTypes::toPixels）
class PhysicsDebugDraw : public b2Draw {
public:
    PhysicsDebugDraw() = default;
    ~PhysicsDebugDraw() override = default;

    // 渲染前设置目标窗口
    void setWindow(sf::RenderWindow* w) { window = w; }

    // 速度可视化：从每个动态 body 质心画一根指向速度方向的线（长度∝速度大小）
    void drawVelocities(b2World* world);

    // b2Draw 接口
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override;
    void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override;
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
    void DrawTransform(const b2Transform& xf) override;
    void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override;

private:
    static sf::Color toSfColor(const b2Color& c);

    sf::RenderWindow* window{};
};

} // namespace physics

#endif // SERVER_BUILD
