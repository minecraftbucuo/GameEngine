//
// Created by MINEC on 2026/6/22.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Toggle.h"
#include "Render/Renderer.h"
#include "Scene.h"
#include <cmath>

Toggle::Toggle(float x, float y, float w, float h, bool initialState) {
    position = {x, y};
    size = {w, h};
    state = initialState;
    currentKnobX = state ? (x + w - h * 0.5f) : (x + h * 0.5f);
    targetKnobX = currentKnobX;

    this->tag = "Toggle:" + std::to_string(this->id);
    className = "Toggle";
}

void Toggle::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    float t = 1.f - std::exp(-lerpSpeed * deltaTime.asSeconds());
    currentKnobX += (targetKnobX - currentKnobX) * t;
}

void Toggle::render(eng::Renderer& renderer) {
    const float h = size.y;
    const float r = h * 0.5f;
    const eng::Color trackColor = state ? trackOnColor : trackOffColor;

    // 轨道：左右两端圆 + 中间矩形（同色重叠拼合，与原 CircleShape 行为一致）
    renderer.drawCircle(eng::Vec2f(position.x + r, position.y + r), r, trackColor);
    renderer.drawRect(eng::FloatRect(position.x + r, position.y, size.x - h, h), trackColor);
    renderer.drawCircle(eng::Vec2f(position.x + size.x - r, position.y + r), r, trackColor);

    // 滑块（圆形）
    const float knobRadius = r * 0.75f;
    renderer.drawCircle(eng::Vec2f(currentKnobX, position.y + r), knobRadius, knobColor);
}

void Toggle::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::MouseButtonPress && event.mouseButton == eng::MouseButton::Left) {
        if (isMouseOver()) {
            state = !state;
            targetKnobX = state ? (position.x + size.x - size.y * 0.5f) : (position.x + size.y * 0.5f);
            if (onToggle) {
                onToggle(state);
            }
        }
    }
}

void Toggle::setState(bool state) {
    this->state = state;
    targetKnobX = state ? (position.x + size.x - size.y * 0.5f) : (position.x + size.y * 0.5f);
    currentKnobX = targetKnobX;
}

bool Toggle::getState() const {
    return state;
}

void Toggle::setOnToggle(std::function<void(bool)>&& callback) {
    onToggle = std::move(callback);
}

bool Toggle::isMouseOver() const {
    eng::FloatRect bounds(position, size);
    eng::Vec2i mousePos = getScene()->getMousePosition();
    return bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
}

#endif
