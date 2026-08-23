//
// Created by MINEC on 2026/8/23.
//
#pragma once

#include "Core/KeyCodes.h"
#include "Core/Types.h"

// 引擎自有事件类型（SDL3 迁移 Step 2）
// 覆盖现有代码使用的全部 SFML 事件：Closed / Resized / Key* / MouseButton* /
// MouseMoved / MouseWheelScrolled / TextEntered（+焦点事件备用）。
// 各字段仅在对应 type 下有效，注释标明。
namespace eng {

enum class EventType {
    KeyPress, KeyRelease,
    MouseButtonPress, MouseButtonRelease, MouseMove, MouseWheel,
    TextEntered,
    WindowResize, WindowClose,
    GainFocus, LostFocus,
};

enum class MouseButton {
    Left, Right, Middle
};

struct EngineEvent {
    EventType type;

    Key key = Key::Unknown;          // KeyPress / KeyRelease 有效
    MouseButton mouseButton{};       // MouseButtonPress / MouseButtonRelease 有效
    eng::Vec2i mousePos;             // MouseMove / MouseButton* / MouseWheel 有效（窗口客户区坐标）
    float wheelDelta = 0.f;          // MouseWheel 有效（>0 向上滚）
    char32_t codepoint = 0;          // TextEntered 有效（Unicode 码点；8=Backspace 等控制码原样透传）
    eng::Vec2u newSize;              // WindowResize 有效
};

} // namespace eng
