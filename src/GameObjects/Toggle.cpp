//
// Created by MINEC on 2026/6/22.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Toggle.h"
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

void Toggle::render(sf::RenderWindow* window) {
    const float h = size.y;
    const float r = h * 0.5f;
    eng::Color trackColor = state ? trackOnColor : trackOffColor;

    // 左半圆
    sf::CircleShape leftCap(r);
    leftCap.setFillColor(trackColor);
    leftCap.setPosition(position.x, position.y);
    window->draw(leftCap);

    // 中间矩形（覆盖左右圆之间的区域）
    sf::RectangleShape middle(eng::Vec2f(size.x - h, h));
    middle.setPosition(position.x + r, position.y);
    middle.setFillColor(trackColor);
    window->draw(middle);

    // 右半圆
    sf::CircleShape rightCap(r);
    rightCap.setFillColor(trackColor);
    rightCap.setPosition(position.x + size.x - h, position.y);
    window->draw(rightCap);

    // 滑块（圆形）
    float knobRadius = r * 0.75f;
    sf::CircleShape knob(knobRadius);
    knob.setFillColor(knobColor);
    knob.setPosition(currentKnobX - knobRadius, position.y + r - knobRadius);
    window->draw(knob);
}

void Toggle::handleEvent(sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
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
