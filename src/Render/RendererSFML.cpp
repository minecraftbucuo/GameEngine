//
// Created by MINEC on 2026/8/23.
//
#ifndef SERVER_BUILD

// 【临时脚手架文件 — SDL3 迁移 Step 10 被 RendererSDL3.cpp 整体替换，Step 11 删除】
// Renderer 的 SFML 实现：内部持有 sf::RenderWindow，绘制命令逐条转发 SFML 图形 API。
#include "Render/Renderer.h"

#include <algorithm>
#include <cmath>
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

void Renderer::setSize(const Vec2u size) {
    if (window) window->setSize(size);
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
                           const float rotationDeg, const Vec2f origin, const Color tint,
                           const bool flipX) {
    if (!window || !h.isValid()) return;
    if (src.width <= 0.f || src.height <= 0.f) return;
    const sf::Texture& texture = AssetManager::getInstance().getTexture(h);
    sf::Sprite sprite(texture);
    // src 原样采样（镜像绝不能翻 src——sprite 图集会取到无关贴图）
    sprite.setTextureRect(sf::IntRect(static_cast<int>(src.left), static_cast<int>(src.top),
                                      static_cast<int>(src.width), static_cast<int>(src.height)));
    sprite.setColor(tint);
    // sprite 分解变换保持默认（position 0 / origin 0 / scale 1 / rotation 0），
    // 用 RenderStates 仿射一次到位。语义与 SDL_RenderCopyRotF(center+flip) 一致：
    // dst 为最终显示矩形，flipX 只镜像 dst 内的内容，origin 为 dst 内旋转支点。
    // 变换链：T(C) * R(θ) * T(B-C) * [M] * S
    //   C = 支点世界位置(dst.lefttop+origin)；B = 镜像基准(dst.righttop，非镜像时 dst.lefttop)
    const float sx = dst.width / src.width;
    const float sy = dst.height / src.height;
    const Vec2f C(dst.left + origin.x, dst.top + origin.y);
    const Vec2f B(flipX ? dst.left + dst.width : dst.left, dst.top);
    sf::Transform t;
    t.translate(C)
     .rotate(rotationDeg)
     .translate(B.x - C.x, B.y - C.y);
    if (flipX) t.scale(-1.f, 1.f);
    t.scale(sx, sy);
    window->draw(sprite, sf::RenderStates(t));
}

void Renderer::drawRect(const FloatRect& r, const Color fillColor, const bool filled,
                        const float outlineThickness, const Color outlineColor,
                        const float rotationDeg, const Vec2f origin) {
    if (!window) return;
    sf::RectangleShape shape(Vec2f(r.width, r.height));
    shape.setPosition(r.left + origin.x, r.top + origin.y);
    shape.setOrigin(origin);
    if (rotationDeg != 0.f) shape.setRotation(rotationDeg);
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

void Renderer::drawCircle(const Vec2f center, const float radius, const Color c, const bool filled,
                          const float outlineThickness, const Color outlineColor) {
    if (!window) return;
    sf::CircleShape shape(radius);
    // SFML CircleShape 的 position 是外接方形左上角，换算为中心定位
    shape.setPosition(center.x - radius, center.y - radius);
    shape.setFillColor(filled ? c : Color::Transparent);
    // 非填充时保证至少有 1px 轮廓可见（与迁移前 SFML 行为一致）
    const float thickness = outlineThickness != 0.f ? outlineThickness : (filled ? 0.f : 1.f);
    if (thickness != 0.f) {
        shape.setOutlineColor(outlineThickness != 0.f ? outlineColor : c);
        shape.setOutlineThickness(thickness);
    }
    window->draw(shape);
}

// 实心圆角矩形（矩形拼直边 + TriangleFan 画四角）；描边 = 先画外扩层再画填充层覆盖
static void fillRoundedRect(sf::RenderWindow* window, const eng::FloatRect& r, const float radius,
                            const sf::Color color) {
    const float rad = std::min(radius, std::min(r.width, r.height) * 0.5f);
    const float x = r.left, y = r.top, w = r.width, h = r.height;

    // 中间水平矩形 + 左右矩形（覆盖四角之间的直边区域）
    sf::RectangleShape rects[3] = {
        sf::RectangleShape(eng::Vec2f(w - 2 * rad, h)),
        sf::RectangleShape(eng::Vec2f(rad, h - 2 * rad)),
        sf::RectangleShape(eng::Vec2f(rad, h - 2 * rad)),
    };
    rects[0].setPosition(x + rad, y);
    rects[1].setPosition(x, y + rad);
    rects[2].setPosition(x + w - rad, y + rad);
    for (auto& rect : rects) {
        rect.setFillColor(color);
        window->draw(rect);
    }

    // 四角 TriangleFan（角度：0=右, PI/2=上；SFML y 向下故 sin 取反）
    constexpr int cornerPoints = 8;
    constexpr float PI = 3.14159265f;
    const struct { eng::Vec2f center; float a0, a1; } corners[] = {
        {{x + w - rad, y + rad},      0.f,          PI / 2.f},     // 右上
        {{x + w - rad, y + h - rad},  3 * PI / 2.f, 2 * PI},       // 右下
        {{x + rad,      y + h - rad}, PI,           3 * PI / 2.f}, // 左下
        {{x + rad,      y + rad},     PI / 2.f,     PI},           // 左上
    };
    for (const auto& corner : corners) {
        sf::VertexArray va(sf::TriangleFan);
        va.append(sf::Vertex(corner.center, color));
        for (int i = 0; i <= cornerPoints; ++i) {
            const float angle = corner.a0 + (corner.a1 - corner.a0) * i / cornerPoints;
            va.append(sf::Vertex(
                eng::Vec2f(corner.center.x + std::cos(angle) * rad,
                           corner.center.y - std::sin(angle) * rad), color));
        }
        window->draw(va);
    }
}

void Renderer::drawRoundedRect(const FloatRect& r, const float radius, const Color fillColor,
                               const float outlineThickness, const Color outlineColor) {
    if (!window) return;
    if (outlineThickness > 0.f) {
        fillRoundedRect(window,
                        FloatRect(r.left - outlineThickness, r.top - outlineThickness,
                                  r.width + outlineThickness * 2, r.height + outlineThickness * 2),
                        radius + outlineThickness, outlineColor);
    }
    fillRoundedRect(window, r, radius, fillColor);
}

void Renderer::drawText(const FontHandle h, const std::string& text, const Vec2f pos,
                        const float size, const Color c, const float scale) {
    if (!window || !h.isValid()) return;
    const sf::Font& font = AssetManager::getInstance().getFont(h);
    const sf::String str = sf::String::fromUtf8(text.begin(), text.end());
    // 光栅化字号固定（size 四舍五入到整数，残差并入变换 scale）：
    // 动画期间调用方保持 size 不变、只动 scale ⇒ 字形不重新光栅化，GPU 平滑拉伸
    const unsigned baseSize = std::max(1u, static_cast<unsigned>(std::lround(size)));
    sf::Text t(str, font, baseSize);
    const float totalScale = size / static_cast<float>(baseSize) * scale;
    if (totalScale != 1.f) t.setScale(totalScale, totalScale);
    t.setPosition(pos);
    t.setFillColor(c);
    window->draw(t);
}

Vec2f Renderer::measureText(const FontHandle h, const std::string& text, const float size,
                            const float scale) {
    if (!h.isValid()) return {};
    const sf::Font& font = AssetManager::getInstance().getFont(h);
    const sf::String str = sf::String::fromUtf8(text.begin(), text.end());
    const unsigned baseSize = std::max(1u, static_cast<unsigned>(std::lround(size)));
    const sf::Text t(str, font, baseSize);
    const sf::FloatRect bounds = t.getGlobalBounds();
    const float totalScale = size / static_cast<float>(baseSize) * scale;
    return {bounds.width * totalScale, bounds.height * totalScale};
}

// ── 相机 ──

void Renderer::setCamera(const Vec2f center, const Vec2f size, const float zoom) {
    if (!window) return;
    sf::View view(center, size);
    if (zoom != 1.f) view.zoom(zoom);
    window->setView(view);
}

Renderer::CameraState Renderer::getCamera() const {
    if (!window) return {};
    const sf::View& view = window->getView();
    return {view.getCenter(), view.getSize(), 1.f};
}

void Renderer::resetCamera() {
    if (window) window->setView(window->getDefaultView());
}

Vec2f Renderer::screenToWorld(const Vec2i screenPos) const {
    if (!window) return Vec2f(static_cast<float>(screenPos.x), static_cast<float>(screenPos.y));
    return window->mapPixelToCoords(screenPos);
}

// SDL3 迁移 6e：getSfmlWindow 过渡 API 已删除——渲染器窗口成为私有实现细节

} // namespace eng

#endif // SERVER_BUILD
