//
// Created by MINEC on 2026/6/22.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <functional>
#include "GameObject.h"

class TextInput : public GameObject {
public:
    TextInput(float x, float y, float w, float h, const sf::String& placeholder = "");

    void update(eng::Time deltaTime) override;
    void render(sf::RenderWindow* window) override;
    void handleEvent(sf::Event& event) override;

    void setString(const sf::String& str);
    [[nodiscard]] sf::String getString() const;

    void setOnConfirm(std::function<void(const sf::String&)>&& callback);

    // 限制可输入字符集（为空则不限制）
    void setAllowedChars(const std::string& chars);

private:
    bool isMouseOver() const;

    eng::Vec2f position;
    eng::Vec2f size;
    float cornerRadius = 8.f;

    sf::String text;
    sf::String placeholder;
    sf::Text displayText;
    sf::Text placeholderText;

    bool focused = false;

    // 光标
    sf::RectangleShape cursor;
    float cursorBlinkTimer = 0.f;
    bool cursorVisible = true;
    static constexpr float BLINK_INTERVAL = 0.5f;

    // 颜色
    eng::Color bgColor = {40, 44, 52};
    eng::Color focusedBgColor = {50, 55, 65};
    eng::Color outlineColor = {80, 85, 95};
    eng::Color focusedOutlineColor = {137, 180, 255};
    eng::Color textColor = {205, 214, 244};
    eng::Color placeholderColor = {100, 108, 128};

    // 回调
    std::function<void(const sf::String&)> onConfirm;

    // 允许输入的字符集（空字符串=不限制）
    std::string allowedChars;
};
#endif
