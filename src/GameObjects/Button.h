//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <functional>
#include <string>

#include "GameObject.h"
#include "Render/Handles.h"

class Button : public GameObject {
public:
    Button(float x, float y, float w, float h, const std::string& button_text = "Button");

    void update(eng::Time deltaTime) override;

    void render(eng::Renderer& renderer) override;

    void handleEvent(const eng::EngineEvent& event) override;

    bool isMouseOver() const;

    void setOnClick(std::function<void()>&& _onClick);

    void setToRectCenter(float x, float y, float w, float h);

    void runOnClick() const;

private:
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

    // 文字（SDL3 迁移 6d：sf::Text 数据化为句柄 + UTF-8 文本，渲染时动态布局）
    eng::FontHandle font;
    std::string label;
    static constexpr unsigned BASE_FONT_SIZE = 30;   // 测量基准字号，实际字号 = 基准 × 缩放

    // 边框
    float outlineThickness = 2.f;

    // 回调
    std::function<void()> onClick;
};
#endif
