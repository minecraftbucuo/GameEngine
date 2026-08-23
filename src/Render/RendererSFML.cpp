//
// Created by MINEC on 2026/8/23.
//
#ifndef SERVER_BUILD

// 【临时脚手架文件 — SDL3 迁移 Step 10 被 RendererSDL3.cpp 整体替换，Step 11 删除】
// Renderer 的 SFML 实现：内部持有 sf::RenderWindow，绘制命令逐条转发 SFML 图形 API。
#include "Render/Renderer.h"

#include <SFML/Graphics.hpp>

#include "Core/EventConvertSFML.h"
#include "Manager/AssetManager.h"

namespace eng {

Renderer::~Renderer() {
    destroyWindow();
}

// ── 窗口 ──

bool Renderer::createWindow(const Vec2u size, const std::string& title) {
    if (window) return true;   // 已创建
    window = new sf::RenderWindow(sf::VideoMode(size.x, size.y), title);
    detail::setInputWindow(window);   // 注册轮询窗口（eng::Input::getMousePosition 用）
    return true;
}

void Renderer::destroyWindow() {
    delete window;
    window = nullptr;
    detail::setInputWindow(nullptr);
}

void Renderer::closeWindow() {
    if (window) window->close();
}

bool Renderer::isWindowOpen() const {
    return window && window->isOpen();
}

Vec2u Renderer::getSize() const {
    return window ? window->getSize() : Vec2u(0, 0);
}

void Renderer::setFramerateLimit(const unsigned fps) {
    if (window) window->setFramerateLimit(fps);
}

// ── 事件泵 ──

bool Renderer::pollEvent(EngineEvent& out) {
    if (!window) return false;
    sf::Event sfEvent{};
    while (window->pollEvent(sfEvent)) {
        // SFML 特有事件（摇杆/传感器等）引擎不关心，跳过后继续轮询
        if (const auto e = toEngineEvent(sfEvent)) {
            out = *e;
            return true;
        }
    }
    return false;
}

// ── 帧控制 ──

void Renderer::clear(const Color c) {
    if (window) window->clear(c);
}

void Renderer::present() {
    if (window) window->display();
}

// ── 绘制命令 ──

void Renderer::drawTexture(const TextureHandle h, const FloatRect& src, const FloatRect& dst,
                           const float rotationDeg, const Vec2f origin, const Color tint) {
    if (!window || !h.isValid()) return;
    const sf::Texture& texture = AssetManager::getInstance().getTexture(h);
    sf::Sprite sprite(texture);
    sprite.setTextureRect(sf::IntRect(static_cast<int>(src.left), static_cast<int>(src.top),
                                      static_cast<int>(src.width), static_cast<int>(src.height)));
    sprite.setPosition(dst.left, dst.top);
    if (src.width > 0.f && src.height > 0.f) {
        sprite.setScale(dst.width / src.width, dst.height / src.height);
    }
    sprite.setRotation(rotationDeg);
    sprite.setOrigin(origin);
    sprite.setColor(tint);
    window->draw(sprite);
}

void Renderer::drawRect(const FloatRect& r, const Color fillColor, const bool filled,
                        const float outlineThickness, const Color outlineColor) {
    if (!window) return;
    sf::RectangleShape shape(Vec2f(r.width, r.height));
    shape.setPosition(r.left, r.top);
    shape.setFillColor(filled ? fillColor : Color::Transparent);
    if (outlineThickness != 0.f) {
        shape.setOutlineColor(outlineColor);
        shape.setOutlineThickness(outlineThickness);
    }
    window->draw(shape);
}

void Renderer::drawLine(const Vec2f a, const Vec2f b, const Color c) {
    if (!window) return;
    sf::Vertex line[] = {
        sf::Vertex(a, c),
        sf::Vertex(b, c)
    };
    window->draw(line, 2, sf::Lines);
}

void Renderer::drawLines(const std::vector<Vec2f>& points, const Color c) {
    if (!window || points.size() < 2) return;
    std::vector<sf::Vertex> vertices;
    vertices.reserve(points.size());
    for (const auto& p : points) {
        vertices.emplace_back(p, c);
    }
    window->draw(vertices.data(), vertices.size(), sf::LineStrip);
}

void Renderer::drawPolygon(const std::vector<Vec2f>& points, const Color c) {
    if (!window || points.size() < 3) return;
    sf::ConvexShape shape(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        shape.setPoint(i, points[i]);
    }
    shape.setFillColor(c);
    window->draw(shape);
}

void Renderer::drawCircle(const Vec2f center, const float radius, const Color c, const bool filled) {
    if (!window) return;
    sf::CircleShape shape(radius);
    // SFML CircleShape 的 position 是外接方形左上角，换算为中心定位
    shape.setPosition(center.x - radius, center.y - radius);
    if (filled) {
        shape.setFillColor(c);
    } else {
        shape.setFillColor(Color::Transparent);
        shape.setOutlineColor(c);
        shape.setOutlineThickness(1.f);
    }
    window->draw(shape);
}

void Renderer::drawText(const FontHandle h, const std::string& text, const Vec2f pos,
                        const unsigned size, const Color c) {
    if (!window || !h.isValid()) return;
    const sf::Font& font = AssetManager::getInstance().getFont(h);
    const sf::String str = sf::String::fromUtf8(text.begin(), text.end());
    sf::Text t(str, font, size);
    t.setPosition(pos);
    t.setFillColor(c);
    window->draw(t);
}

// ── 相机 ──

void Renderer::setCamera(const Vec2f center, const Vec2f size, const float zoom) {
    if (!window) return;
    sf::View view(center, size);
    if (zoom != 1.f) view.zoom(zoom);
    window->setView(view);
}

void Renderer::resetCamera() {
    if (window) window->setView(window->getDefaultView());
}

Vec2f Renderer::screenToWorld(const Vec2i screenPos) const {
    if (!window) return Vec2f(static_cast<float>(screenPos.x), static_cast<float>(screenPos.y));
    return window->mapPixelToCoords(screenPos);
}

// getSfmlWindow 为 Renderer.h 类内 inline 定义（服务端构建无本文件也能链接）

} // namespace eng

#endif // SERVER_BUILD
