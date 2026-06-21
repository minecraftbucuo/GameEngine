//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD

#include "Button.h"
#include "AssetManager.h"
#include "GameObject.h"
#include "Scene.h"
#include <cmath>

Button::Button(const float x, const float y, const float w, const float h, const sf::String& button_text) {
    position = {x, y};
    size = {w, h};

    text.setFont(AssetManager::getInstance().getFont());
    text.setString(button_text);
    const float scale = std::min(h * 0.7f / text.getLocalBounds().height, w * 0.7f / text.getLocalBounds().width);
    text.setScale(scale, scale);
    text.setFillColor({30, 30, 46});
    text.setPosition(x + (w - text.getGlobalBounds().width) * 0.5f,
                     y + (h - text.getGlobalBounds().height) * 0.5f);

    textShadow = text;
    textShadow.setFillColor({0, 0, 0, 80});
    textShadow.setPosition(text.getPosition().x + 1.5f, text.getPosition().y + 1.5f);

    this->tag = "Button:" + std::to_string(this->id);
    className = "Button";
}

void Button::update(sf::Time deltaTime) {
    GameObject::update(deltaTime);
    targetScale = is_hover ? hoverScale : 1.f;
    const float t = 1.f - std::exp(-scaleLerpSpeed * deltaTime.asSeconds());
    currentScale += (targetScale - currentScale) * t;
}

void Button::render(sf::RenderWindow* window) {
    StateColors colors = normalColors;
    if (is_pressed) {
        colors = pressedColors;
    } else if (is_hover) {
        colors = hoverColors;
    }

    // 以按钮中心为原点缩放
    const sf::Vector2f center = position + size * 0.5f;
    const sf::Vector2f scaledSize = size * currentScale;
    const sf::Vector2f scaledPos = center - scaledSize * 0.5f;

    drawRoundedRect(window, scaledPos, scaledSize, cornerRadius * currentScale, colors.fill, colors.outline, outlineThickness);

    // 文字缩放并居中
    const float baseTextScale = std::min(size.y * 0.7f / text.getLocalBounds().height, size.x * 0.7f / text.getLocalBounds().width);
    const float scaledTextScale = baseTextScale * currentScale;
    text.setScale(scaledTextScale, scaledTextScale);
    textShadow.setScale(scaledTextScale, scaledTextScale);

    text.setPosition(center.x - text.getGlobalBounds().width * 0.5f,
                     center.y - text.getGlobalBounds().height * 0.5f);
    textShadow.setPosition(text.getPosition().x + 1.5f, text.getPosition().y + 1.5f);

    window->draw(textShadow);
    window->draw(text);
}

void Button::handleEvent(sf::Event& event) {
    if (event.type == sf::Event::MouseMoved) {
        is_hover = isMouseOver();
    } else if (event.type == sf::Event::MouseButtonPressed) {
        if (is_hover) {
            is_pressed = true;
        }
    } else if (event.type == sf::Event::MouseButtonReleased) {
        if (is_pressed && is_hover && onClick) onClick();
        is_pressed = false;
    }
}

bool Button::isMouseOver() const {
    const sf::FloatRect buttonBounds(position.x, position.y, size.x, size.y);
    const auto mouse_pos = this->getScene()->getMousePosition();
    return buttonBounds.contains(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));
}

void Button::setOnClick(std::function<void()>&& _onClick) {
    this->onClick = std::move(_onClick);
}

void Button::setToRectCenter(const float x, const float y, const float w, const float h) {
    position.x = x + w * 0.5f - size.x * 0.5f;
    position.y = y + h * 0.5f - size.y * 0.5f;
    text.setPosition(x + (w - text.getGlobalBounds().width) * 0.5f,
                     y + (h - text.getGlobalBounds().height) * 0.5f);
    textShadow.setPosition(text.getPosition().x + 1.5f, text.getPosition().y + 1.5f);
}

void Button::runOnClick() const {
    if (onClick) onClick();
}

void Button::drawRoundedRect(sf::RenderWindow* window, sf::Vector2f pos, sf::Vector2f size,
                              float radius, const sf::Color& fillColor, const sf::Color& outlineColor, float outlineThickness) {
    radius = std::min(radius, std::min(size.x, size.y) * 0.5f);

    // 先画边框（更大的圆角矩形），再画填充覆盖，形成边框效果
    if (outlineThickness > 0.f) {
        const sf::Vector2f outPos = pos - sf::Vector2f(outlineThickness, outlineThickness);
        const sf::Vector2f outSize = size + sf::Vector2f(outlineThickness * 2, outlineThickness * 2);
        const float outRadius = radius + outlineThickness;
        drawFilledRoundedRect(window, outPos, outSize, outRadius, outlineColor);
    }
    drawFilledRoundedRect(window, pos, size, radius, fillColor);
}

void Button::drawFilledRoundedRect(sf::RenderWindow* window, sf::Vector2f pos, sf::Vector2f size,
                                     float radius, const sf::Color& color) {
    // 用矩形拼直边 + TriangleFan 画四角
    const float r = radius;
    const float w = size.x;
    const float h = size.y;

    // 中间水平矩形（覆盖左右圆角之间的区域）
    sf::RectangleShape centerRect(sf::Vector2f(w - 2 * r, h));
    centerRect.setPosition(pos.x + r, pos.y);
    centerRect.setFillColor(color);
    window->draw(centerRect);

    // 左侧矩形（覆盖上下圆角之间的区域）
    sf::RectangleShape leftRect(sf::Vector2f(r, h - 2 * r));
    leftRect.setPosition(pos.x, pos.y + r);
    leftRect.setFillColor(color);
    window->draw(leftRect);

    // 右侧矩形
    sf::RectangleShape rightRect(sf::Vector2f(r, h - 2 * r));
    rightRect.setPosition(pos.x + w - r, pos.y + r);
    rightRect.setFillColor(color);
    window->draw(rightRect);

    // 四个圆角用 TriangleFan
    const int cornerPoints = 8;
    constexpr float PI = 3.14159265f;

    struct Corner {
        sf::Vector2f center;
        float startAngle; // 弧度
        float endAngle;
    };

    // 顺时针：右上、右下、左下、左上
    // 角度：0=右, PI/2=上, PI=左, 3PI/2=下（数学坐标系，y向上）
    // SFML y向下，所以 sin 取反
    Corner corners[] = {
        {{pos.x + w - r, pos.y + r},     0.f,         PI / 2.f},    // 右上：0°→90°
        {{pos.x + w - r, pos.y + h - r}, 3*PI / 2.f,  2 * PI},      // 右下：270°→360°
        {{pos.x + r, pos.y + h - r},     PI,          3*PI / 2.f},  // 左下：180°→270°
        {{pos.x + r, pos.y + r},         PI / 2.f,    PI},          // 左上：90°→180°
    };

    for (const auto& corner : corners) {
        sf::VertexArray va(sf::TriangleFan);
        va.append(sf::Vertex(corner.center, color));
        for (int i = 0; i <= cornerPoints; ++i) {
            const float angle = corner.startAngle + (corner.endAngle - corner.startAngle) * i / cornerPoints;
            va.append(sf::Vertex(
                sf::Vector2f(corner.center.x + std::cos(angle) * r,
                             corner.center.y - std::sin(angle) * r),
                color));
        }
        window->draw(va);
    }
}

#endif
