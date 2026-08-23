//
// Created by MINEC on 2026/8/23.
//
#pragma once

#include "Core/KeyCodes.h"
#include "Core/Types.h"

// 引擎输入轮询接口（SDL3 迁移 Step 2 引入）
// 与事件驱动（EngineEvent）互补的实时状态查询。
// 实现位于 RendererSDL3.cpp（SDL_GetKeyboardState / SDL_GetMouseState）。
namespace eng::Input {

// 键盘实时状态：该键当前是否被按住
bool isKeyPressed(Key key);

// 鼠标位置（窗口客户区相对坐标，像素；与 MouseMove 事件坐标同基准）
eng::Vec2i getMousePosition();

} // namespace eng::Input
