//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <functional>

#include "GameObject.h"

class Button : public GameObject {
public:
    Button(float x, float y, float w, float h, const sf::String& button_text = "Button");

    void update(eng::Time deltaTime) override;

    void render(sf::RenderWindow* window) override;

    void handleEvent(sf::Event& event) override;

    bool isMouseOver() const;

    void setOnClick(std::function<void()>&& _onClick);

    void setToRectCenter(float x, float y, float w, float h);

    void runOnClick() const;

private:
    void drawRoundedRect(sf::RenderWindow* window, eng::Vec2f pos, eng::Vec2f size,
                         float radius, const eng::Color& fillColor, const eng::Color& outlineColor, float outlineThickness);

    void drawFilledRoundedRect(sf::RenderWindow* window, eng::Vec2f pos, eng::Vec2f size,
                               float radius, const eng::Color& color);

    // 位置和尺寸
    eng::Vec2f position;
    eng::Vec2f size;
    float cornerRadius = 12.f;

    // 状态
    bool is_hover = false;
    bool is_pressed = false;

    // 缩放动画
    float currentScale = 1.f;
    float targetScale = 1.f;
    float hoverScale = 1.12f;
    float scaleLerpSpeed = 6.f;

    // 三态颜色配置
    struct StateColors {
        eng::Color fill;
        eng::Color outline;
    };
    StateColors normalColors  = {{137, 180, 255}, {116, 199, 255}};
    StateColors hoverColors   = {{166, 255, 161}, {148, 255, 213}};
    StateColors pressedColors = {{40, 120, 70}, {30, 90, 50}};

    // 文字
    sf::Text text;
    sf::Text textShadow;

    // 边框
    float outlineThickness = 2.f;

    // 回调
    std::function<void()> onClick;
};
#endif
