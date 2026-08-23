//
// Created by MINEC on 2026/8/23.
//
#pragma once

// 引擎自有键码（SDL3 迁移 Step 2）
// 覆盖：现有代码使用的全部按键（A/D/W/J/R/Space/Escape/Enter/方向键）
// + 常用键前瞻（字母/数字/F 区/修饰键/编辑键/标点），两个后端（SFML/SDL3 scancode）均可直接映射。
// 语义为物理键位（scancode 心智模型），与布局无关。
namespace eng {

enum class Key {
    Unknown = 0,

    // 字母
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 数字主区
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // 功能键
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // 控制键
    Escape, Enter, Space, Tab, Backspace,
    Insert, Delete, Home, End, PageUp, PageDown,

    // 修饰键（左右分开）
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,

    // 方向键
    Up, Down, Left, Right,

    // 常用标点
    Comma, Period, Slash, Semicolon, Apostrophe,
    LBracket, RBracket, Minus, Equal, Backquote,

    Count
};

} // namespace eng
