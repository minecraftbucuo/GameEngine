//
// Created by MINEC on 2026/6/22.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <functional>
#include "GameObject.h"

class Toggle : public GameObject {
public:
    Toggle(float x, float y, float w, float h, bool initialState = false);

    void update(eng::Time deltaTime) override;

    void render(eng::Renderer& renderer) override;
    void handleEvent(const eng::EngineEvent& event) override;

    void setState(bool state);
    [[nodiscard]] bool getState() const;

    void setOnToggle(std::function<void(bool)>&& callback);

    // public 化的定位（基类版本为 protected）：滑块为绝对坐标，随位置一并重算，
    // 供场景在视口宽度自适应后重排布局
    void setPosition(const float posX, const float posY) override {
        position = eng::Vec2f(posX, posY);
        currentKnobX = state ? (position.x + size.x - size.y * 0.5f)
                             : (position.x + size.y * 0.5f);
        targetKnobX = currentKnobX;
        GameObject::setPosition(posX, posY);
    }

private:
    bool isMouseOver() const;

    eng::Vec2f position;
    eng::Vec2f size;

    bool state = false;

    // 滑块动画
    float currentKnobX = 0.f;
    float targetKnobX = 0.f;
    float lerpSpeed = 10.f;

    // 颜色
    eng::Color trackOffColor = {80, 85, 95};
    eng::Color trackOnColor = {137, 180, 255};
    eng::Color knobColor = {205, 214, 244};

    // 回调
    std::function<void(bool)> onToggle;
};
#endif
