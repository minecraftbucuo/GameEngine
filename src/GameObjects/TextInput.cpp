//
// Created by MINEC on 2026/6/22.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "TextInput.h"
#include "AssetManager.h"
#include "Render/Renderer.h"
#include "Scene.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstddef>

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

// 获取前一个 codepoint 的开头字节偏移
size_t TextInput::utf8PrevByteOffset(const std::string& s, size_t bytePos) {
    if (bytePos == 0) return 0;
    size_t pos = bytePos;
    do {
        --pos;
    } while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80);
    return pos;
}

// 获取后一个 codepoint 的开头字节偏移
size_t TextInput::utf8NextByteOffset(const std::string& s, size_t bytePos) {
    if (bytePos >= s.size()) return s.size();
    size_t pos = bytePos + 1;
    while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
        ++pos;
    }
    return pos;
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

    const float textOffsetX = position.x + 10.f;
    const float textOffsetY = position.y + (size.y - static_cast<float>(FONT_SIZE)) * 0.5f;

    // 文本或占位符
    if (text.empty()) {
        renderer.drawText(font, placeholder, {textOffsetX, textOffsetY}, FONT_SIZE, placeholderColor);
    } else {
        // 选中高亮
        if (hasSelection() && focused) {
            const size_t selStart = std::min(selectionStart, cursorPos);
            const size_t selEnd = std::max(selectionStart, cursorPos);

            const float selStartWidth = renderer.measureText(font, text.substr(0, selStart), FONT_SIZE).x;
            const float selEndWidth = renderer.measureText(font, text.substr(0, selEnd), FONT_SIZE).x;

            renderer.drawRect(
                eng::FloatRect(textOffsetX + selStartWidth,
                               position.y + (size.y - size.y * 0.6f) * 0.5f,
                               selEndWidth - selStartWidth,
                               size.y * 0.6f),
                selectionColor);
        }

        // 绘制文本
        renderer.drawText(font, text, {textOffsetX, textOffsetY}, FONT_SIZE, textColor);

        // 光标：显示在 cursorPos 处
        if (focused && cursorVisible) {
            const float textWidth = renderer.measureText(font, text.substr(0, cursorPos), FONT_SIZE).x;
            renderer.drawRect(eng::FloatRect(textOffsetX + textWidth,
                                             position.y + (size.y - size.y * 0.6f) * 0.5f,
                                             2.f, size.y * 0.6f), textColor);
        }
    }
}

void TextInput::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::MouseButtonPress) {
        if (event.mouseButton == eng::MouseButton::Left) {
            if (isMouseOver()) {
                focused = true;
                // 点击聚焦时光标移到末尾，清除选中
                cursorPos = text.size();
                selectionStart = cursorPos;
                cursorBlinkTimer = 0.f;
                cursorVisible = true;
            } else {
                focused = false;
            }
        }
    }

    if (!focused) return;

    // ── 修饰键状态跟踪 ──
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::LCtrl || event.key == eng::Key::RCtrl) {
            ctrlDown = true;
        }
        if (event.key == eng::Key::LShift || event.key == eng::Key::RShift) {
            shiftDown = true;
        }
    }
    if (event.type == eng::EventType::KeyRelease) {
        if (event.key == eng::Key::LCtrl || event.key == eng::Key::RCtrl) {
            ctrlDown = false;
        }
        if (event.key == eng::Key::LShift || event.key == eng::Key::RShift) {
            shiftDown = false;
        }
        return; // KeyRelease 处理完修饰键就返回
    }

    // ── KeyPress 处理 ──
    if (event.type == eng::EventType::KeyPress) {
        switch (event.key) {
        case eng::Key::Left:
            if (!shiftDown) clearSelection();
            moveCursorLeft();
            break;
        case eng::Key::Right:
            if (!shiftDown) clearSelection();
            moveCursorRight();
            break;
        case eng::Key::Home:
            if (!shiftDown) clearSelection();
            moveCursorToStart();
            break;
        case eng::Key::End:
            if (!shiftDown) clearSelection();
            moveCursorToEnd();
            break;
        case eng::Key::Delete:
            if (hasSelection()) {
                deleteSelection();
            } else {
                deleteAtCursor();
            }
            break;
        case eng::Key::Backspace:
            // Backspace 通过 TextEntered 的 codePoint=8 处理
            // 这里不要重复处理
            break;
        case eng::Key::Enter:
            if (onConfirm) {
                onConfirm(text);
            }
            focused = false;
            break;
        case eng::Key::Escape:
            focused = false;
            break;
        default:
            break;
        }

        // ── Ctrl 组合键 ──
        if (ctrlDown) {
            switch (event.key) {
            case eng::Key::A:
                selectAll();
                break;
            case eng::Key::C:
                if (hasSelection()) {
                    const std::string sel = getSelectedText();
                    SDL_SetClipboardText(sel.c_str());
                }
                break;
            case eng::Key::X:
                if (hasSelection()) {
                    const std::string sel = getSelectedText();
                    SDL_SetClipboardText(sel.c_str());
                    deleteSelection();
                }
                break;
            case eng::Key::V:
                if (char* clipText = SDL_GetClipboardText()) {
                    insertStringAtCursor(clipText);
                    SDL_free(clipText);
                }
                break;
            default:
                break;
            }
        }

        cursorBlinkTimer = 0.f;
        cursorVisible = true;
        return;
    }

    // ── TextEntered 处理 ──
    if (event.type == eng::EventType::TextEntered) {
        const char32_t codePoint = event.codepoint;

        // 退格（通过 TextEntered 的 codePoint=8 统一处理）
        if (codePoint == 8) {
            if (hasSelection()) {
                deleteSelection();
            } else {
                backspaceAtCursor();
            }
            cursorBlinkTimer = 0.f;
            cursorVisible = true;
            return;
        }

        // 过滤控制字符
        if (codePoint < 32) return;

        // 字符集过滤（allowedChars 为 ASCII 集合，非 ASCII 一律拒绝）
        if (!allowedChars.empty()) {
            if (codePoint >= 128 || allowedChars.find(static_cast<char>(codePoint)) == std::string::npos) {
                return;
            }
        }

        // 如果有选中，先删选中再插入
        if (hasSelection()) {
            deleteSelection();
        }

        insertAtCursor(codePoint);
        cursorBlinkTimer = 0.f;
        cursorVisible = true;
    }
}

void TextInput::setString(const std::string& str) {
    text = str;
    cursorPos = text.size();
    selectionStart = cursorPos;
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

// ── 光标移动 ──

void TextInput::moveCursorLeft() {
    if (cursorPos == 0) return;
    if (shiftDown) {
        // Shift+Left 扩展选中
        cursorPos = utf8PrevByteOffset(text, cursorPos);
    } else {
        cursorPos = utf8PrevByteOffset(text, cursorPos);
        selectionStart = cursorPos;
    }
}

void TextInput::moveCursorRight() {
    if (cursorPos >= text.size()) return;
    if (shiftDown) {
        cursorPos = utf8NextByteOffset(text, cursorPos);
    } else {
        cursorPos = utf8NextByteOffset(text, cursorPos);
        selectionStart = cursorPos;
    }
}

void TextInput::moveCursorToStart() {
    if (shiftDown) {
        cursorPos = 0;
    } else {
        cursorPos = 0;
        selectionStart = 0;
    }
}

void TextInput::moveCursorToEnd() {
    if (shiftDown) {
        cursorPos = text.size();
    } else {
        cursorPos = text.size();
        selectionStart = text.size();
    }
}

// ── 选中操作 ──

void TextInput::selectAll() {
    selectionStart = 0;
    cursorPos = text.size();
}

bool TextInput::hasSelection() const {
    return selectionStart != cursorPos;
}

void TextInput::clearSelection() {
    selectionStart = cursorPos;
}

std::string TextInput::getSelectedText() const {
    if (!hasSelection()) return {};
    const size_t start = std::min(selectionStart, cursorPos);
    const size_t end = std::max(selectionStart, cursorPos);
    return text.substr(start, end - start);
}

void TextInput::deleteSelection() {
    if (!hasSelection()) return;
    const size_t start = std::min(selectionStart, cursorPos);
    const size_t end = std::max(selectionStart, cursorPos);
    text.erase(start, end - start);
    cursorPos = start;
    selectionStart = start;
}

// ── 插入 ──

void TextInput::insertAtCursor(char32_t cp) {
    std::string encoded;
    utf8Append(encoded, cp);
    text.insert(cursorPos, encoded);
    cursorPos += encoded.size();
    selectionStart = cursorPos; // 插入后清除选中
}

void TextInput::insertStringAtCursor(const std::string& str) {
    if (str.empty()) return;
    if (hasSelection()) deleteSelection();

    // 如果有 allowedChars 限制，过滤掉不允许的字符
    std::string filtered;
    if (!allowedChars.empty()) {
        for (const unsigned char c : str) {
            if (c < 128 && allowedChars.find(static_cast<char>(c)) != std::string::npos) {
                filtered += static_cast<char>(c);
            }
        }
    } else {
        filtered = str;
    }

    if (filtered.empty()) return;
    text.insert(cursorPos, filtered);
    cursorPos += filtered.size();
    selectionStart = cursorPos;
}

// ── 删除 ──

void TextInput::backspaceAtCursor() {
    if (cursorPos == 0) return;
    const size_t prev = utf8PrevByteOffset(text, cursorPos);
    text.erase(prev, cursorPos - prev);
    cursorPos = prev;
    selectionStart = cursorPos;
}

void TextInput::deleteAtCursor() {
    if (cursorPos >= text.size()) return;
    const size_t next = utf8NextByteOffset(text, cursorPos);
    text.erase(cursorPos, next - cursorPos);
    selectionStart = cursorPos;
}

#endif