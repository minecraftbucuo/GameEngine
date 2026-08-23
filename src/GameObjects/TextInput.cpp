//
// Created by MINEC on 2026/6/22.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "TextInput.h"
#include "AssetManager.h"
#include "Render/Renderer.h"
#include "Scene.h"
#include <cmath>

// ── UTF-8 codepoint 编解码（SDL3 迁移 6d：sf::String UTF-32 存储改为 UTF-8）──

// 追加一个 codepoint（非法范围编码为 U+FFFD）
static void utf8Append(std::string& s, const char32_t cp) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        s += "\xEF\xBF\xBD";
        return;
    }
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// 删除末尾一个 codepoint（退格）
static void utf8PopBack(std::string& s) {
    if (s.empty()) return;
    s.pop_back();
    // 续字节（10xxxxxx）继续删，直到前导字节被删掉
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) {
        s.pop_back();
    }
}

TextInput::TextInput(float x, float y, float w, float h, const std::string& placeholder) {
    position = {x, y};
    size = {w, h};
    this->placeholder = placeholder;
    font = AssetManager::getInstance().getFontHandle();

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

void TextInput::render(eng::Renderer& renderer) {
    // 背景（含 1.5px 描边）
    const eng::Color bg = focused ? focusedBgColor : bgColor;
    const eng::Color outline = focused ? focusedOutlineColor : outlineColor;
    renderer.drawRect(eng::FloatRect(position, size), bg, true, 1.5f, outline);

    // 文本或占位符（垂直居中：字号 16，基线偏移按迁移前 (h-16)/2 布局）
    const eng::Vec2f textPos(position.x + 10.f, position.y + (size.y - static_cast<float>(FONT_SIZE)) * 0.5f);
    if (text.empty()) {
        renderer.drawText(font, placeholder, textPos, FONT_SIZE, placeholderColor);
    } else {
        renderer.drawText(font, text, textPos, FONT_SIZE, textColor);
    }

    // 光标：x = 文本宽度处
    if (focused && cursorVisible) {
        float textWidth = 0.f;
        if (!text.empty()) {
            textWidth = renderer.measureText(font, text, FONT_SIZE).x;
        }
        renderer.drawRect(eng::FloatRect(position.x + 10.f + textWidth,
                                         position.y + (size.y - size.y * 0.6f) * 0.5f,
                                         2.f, size.y * 0.6f), textColor);
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

        // 字符集过滤（allowedChars 为 ASCII 集合，非 ASCII 一律拒绝，与迁移前一致）
        if (!allowedChars.empty() && codePoint != 8) {
            if (codePoint >= 128 || allowedChars.find(static_cast<char>(codePoint)) == std::string::npos) {
                return;
            }
        }

        // 处理退格
        if (codePoint == 8) {
            if (!text.empty()) {
                utf8PopBack(text);
                cursorBlinkTimer = 0.f;
                cursorVisible = true;
            }
        } else {
            // 添加字符
            utf8Append(text, codePoint);
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

void TextInput::setString(const std::string& str) {
    text = str;
}

const std::string& TextInput::getString() const {
    return text;
}

void TextInput::setOnConfirm(std::function<void(const std::string&)> callback) {
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
