//
// Created by MINEC on 2026/6/22.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "TextInput.h"
#include "AssetManager.h"
#include "Scene.h"
#include <cmath>
#include <algorithm>

TextInput::TextInput(float x, float y, float w, float h, const sf::String& placeholder) {
    position = {x, y};
    size = {w, h};
    this->placeholder = placeholder;

    // 初始化显示文本
    displayText.setFont(AssetManager::getInstance().getFont());
    displayText.setCharacterSize(16);
    displayText.setFillColor(textColor);
    displayText.setPosition(position.x + 10.f, position.y + (h - 16.f) * 0.5f);

    // 初始化占位符文本
    placeholderText = displayText;
    placeholderText.setString(placeholder);
    placeholderText.setFillColor(placeholderColor);

    // 初始化光标
    cursor.setSize(eng::Vec2f(2.f, h * 0.6f));
    cursor.setFillColor(textColor);
    cursor.setPosition(position.x + 10.f, position.y + (h - h * 0.6f) * 0.5f);

    this->tag = "TextInput:" + std::to_string(this->id);
    className = "TextInput";
}

void TextInput::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);

    // 光标闪烁
    if (focused) {
        cursorBlinkTimer += deltaTime.asSeconds();
        if (cursorBlinkTimer >= BLINK_INTERVAL) {
            cursorBlinkTimer = 0.f;
            cursorVisible = !cursorVisible;
        }
    } else {
        cursorVisible = true;
    }
}

void TextInput::render(sf::RenderWindow* window) {
    // 绘制背景
    eng::Color bg = focused ? focusedBgColor : bgColor;
    eng::Color outline = focused ? focusedOutlineColor : outlineColor;

    // 使用矩形绘制
    sf::RectangleShape bgRect(size);
    bgRect.setPosition(position);
    bgRect.setFillColor(bg);
    bgRect.setOutlineColor(outline);
    bgRect.setOutlineThickness(1.5f);
    window->draw(bgRect);

    // 绘制文本或占位符
    if (text.isEmpty()) {
        window->draw(placeholderText);
    } else {
        window->draw(displayText);
    }

    // 绘制光标
    if (focused && cursorVisible) {
        // 计算光标位置
        float textWidth = 0.f;
        if (!text.isEmpty()) {
            eng::FloatRect bounds = displayText.getLocalBounds();
            textWidth = bounds.width;
        }
        cursor.setPosition(position.x + 10.f + textWidth, cursor.getPosition().y);
        window->draw(cursor);
    }
}

void TextInput::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::MouseButtonPress) {
        if (event.mouseButton == eng::MouseButton::Left) {
            if (isMouseOver()) {
                focused = true;
            } else {
                focused = false;
            }
        }
    } else if (event.type == eng::EventType::TextEntered && focused) {
        const char32_t codePoint = event.codepoint;

        // 过滤控制字符（除了退格）
        if (codePoint < 32 && codePoint != 8) {
            return;
        }

        // 字符集过滤
        if (!allowedChars.empty() && codePoint != 8) {
            char ch = static_cast<char>(codePoint);
            if (allowedChars.find(ch) == std::string::npos) {
                return;
            }
        }

        // 处理退格
        if (codePoint == 8) {
            if (!text.isEmpty()) {
                text = text.substring(0, text.getSize() - 1);
                displayText.setString(text);
                cursorBlinkTimer = 0.f;
                cursorVisible = true;
            }
        } else {
            // 添加字符
            text += static_cast<sf::Uint32>(codePoint);
            displayText.setString(text);
            cursorBlinkTimer = 0.f;
            cursorVisible = true;
        }
    } else if (event.type == eng::EventType::KeyPress && focused) {
        if (event.key == eng::Key::Enter) {
            if (onConfirm) {
                onConfirm(text);
            }
            focused = false;
        } else if (event.key == eng::Key::Escape) {
            focused = false;
        }
    }
}

void TextInput::setString(const sf::String& str) {
    text = str;
    displayText.setString(text);
}

sf::String TextInput::getString() const {
    return text;
}

void TextInput::setOnConfirm(std::function<void(const sf::String&)>&& callback) {
    onConfirm = std::move(callback);
}

void TextInput::setAllowedChars(const std::string& chars) {
    allowedChars = chars;
}

bool TextInput::isMouseOver() const {
    eng::FloatRect bounds(position, size);
    eng::Vec2i mousePos = getScene()->getMousePosition();
    return bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
}

#endif
