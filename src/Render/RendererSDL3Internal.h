//
// Created by MINEC on 2026/8/23.
//
#pragma once

// SDL3 实现层内部共享（SDL3 迁移 Step 9，非公共 API）
// AssetManagerSDL3 懒创建纹理时需要当前渲染器；由 RendererSDL3.cpp 提供实现。
typedef struct SDL_Renderer SDL_Renderer;

namespace eng::detail {

// 当前 SDL 渲染器；窗口未创建时为 nullptr
SDL_Renderer* sdl3Renderer();

} // namespace eng::detail
