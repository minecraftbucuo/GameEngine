//
// Created by MINEC on 2026/6/22.
//

#pragma once
#ifndef SERVER_BUILD
#include <functional>
#include "GameObject.h"

class Toggle : public GameObject {
public:
    Toggle(float x, float y, float w, float h, bool initialState = false);

    void update(sf::Time deltaTime) override;
    void render(sf::RenderWindow* window) override;
    void handleEvent(sf::Event& event) override;

    void setState(bool state);
    [[nodiscard]] bool getState() const;

    void setOnToggle(std::function<void(bool)>&& callback);

private:
    bool isMouseOver() const;

    sf::Vector2f position;
    sf::Vector2f size;

    bool state = false;

    // 滑块动画
    float currentKnobX = 0.f;
    float targetKnobX = 0.f;
    float lerpSpeed = 10.f;

    // 颜色
    sf::Color trackOffColor = {80, 85, 95};
    sf::Color trackOnColor = {137, 180, 255};
    sf::Color knobColor = {205, 214, 244};

    // 回调
    std::function<void(bool)> onToggle;
};
#endif
