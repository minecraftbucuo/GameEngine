//
// Created by MINEC on 2026/5/8.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD

#include "Button.h"
#include "AssetManager.h"
#include "GameObject.h"
#include "Render/Renderer.h"
#include "Scene.h"
#include <algorithm>
#include <cmath>

Button::Button(const float x, const float y, const float w, const float h, const std::string& button_text) {
    position = {x, y};
    size = {w, h};

    font = AssetManager::getInstance().getFontHandle();
    label = button_text;

    this->tag = "Button:" + std::to_string(this->id);
    className = "Button";
}

void Button::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    targetScale = is_hover ? hoverScale : 1.f;
    const float t = 1.f - std::exp(-scaleLerpSpeed * deltaTime.asSeconds());
    currentScale += (targetScale - currentScale) * t;
}

void Button::render(eng::Renderer& renderer) {
    StateColors colors = normalColors;
    if (is_pressed) {
        colors = pressedColors;
    } else if (is_hover) {
        colors = hoverColors;
    }

    // 以按钮中心为原点缩放
    const eng::Vec2f center = position + size * 0.5f;
    const eng::Vec2f scaledSize = size * currentScale;
    const eng::Vec2f scaledPos = center - scaledSize * 0.5f;

    renderer.drawRoundedRect(eng::FloatRect(scaledPos, scaledSize),
                             cornerRadius * currentScale, colors.fill,
                             outlineThickness, colors.outline);

    // 文字：与原版 sf::Text 完全一致的渲染策略——
    // 固定基准字号光栅化，适配缩放×hover 动画缩放全部走变换 scale（GPU 平滑拉伸，
    // 不逐帧重新光栅化，字形无抖动变形）
    const eng::Vec2f base = renderer.measureText(font, label, static_cast<float>(BASE_FONT_SIZE));
    const float fitScale = std::min(size.y * 0.7f / base.y, size.x * 0.7f / base.x);
    const float textScale = fitScale * currentScale;
    const eng::Vec2f glyphSize = renderer.measureText(font, label,
                                                      static_cast<float>(BASE_FONT_SIZE), textScale);

    const eng::Vec2f textPos(center - glyphSize * 0.5f);
    const eng::Vec2f shadowOffset(1.5f, 1.5f);
    renderer.drawText(font, label, textPos + shadowOffset, static_cast<float>(BASE_FONT_SIZE),
                      eng::Color(0, 0, 0, 80), textScale);
    renderer.drawText(font, label, textPos, static_cast<float>(BASE_FONT_SIZE),
                      eng::Color(30, 30, 46), textScale);
}

void Button::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::MouseMove) {
        is_hover = isMouseOver();
    } else if (event.type == eng::EventType::MouseButtonPress) {
        if (is_hover) {
            is_pressed = true;
        }
    } else if (event.type == eng::EventType::MouseButtonRelease) {
        if (is_pressed && is_hover && onClick) onClick();
        is_pressed = false;
    }
}

bool Button::isMouseOver() const {
    const eng::FloatRect buttonBounds(position.x, position.y, size.x, size.y);
    const auto mouse_pos = this->getScene()->getMousePosition();
    return buttonBounds.contains(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));
}

void Button::setOnClick(std::function<void()>&& _onClick) {
    this->onClick = std::move(_onClick);
}

void Button::setToRectCenter(const float x, const float y, const float w, const float h) {
    position.x = x + w * 0.5f - size.x * 0.5f;
    position.y = y + h * 0.5f - size.y * 0.5f;
}

void Button::runOnClick() const {
    if (onClick) onClick();
}

#endif
