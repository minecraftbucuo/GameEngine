//
// Created by MINEC on 2026/8/23.
//
#pragma once

// 【临时脚手架文件 — SDL3 迁移 Step 11 删除，届时由 RendererSDL3.cpp 接管实现】
// SFML 事件/输入 → 引擎事件/输入的转换层。
// 头文件仅前向声明 SFML 类型，不引入 SFML include。
#include <optional>

#include "Core/Event.h"

namespace sf {
    class Event;
    class RenderWindow;
}

namespace eng {

// sf::Event → EngineEvent。
// SFML 特有且引擎不关心的事件（摇杆/触屏/传感器等）返回 std::nullopt。
std::optional<EngineEvent> toEngineEvent(const sf::Event& event);

// sf::Keyboard::Key ↔ eng::Key 双向映射（未识别键 → Unknown）
Key toEngineKey(int sfKey);
int toSfKey(Key key);

} // namespace eng

namespace eng::detail {

// 注册 SFML 窗口供 Input::getMousePosition 轮询使用（GameEngine 主循环启动时调用一次）
void setInputWindow(sf::RenderWindow* window);

} // namespace eng::detail
