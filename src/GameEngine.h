//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <memory>
#include "SceneManager.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"
#endif

class GameEngine {
public:
    GameEngine() = default;
    ~GameEngine();

    void init();
#ifndef SERVER_BUILD
    void start();
#else
    [[noreturn]] void start();
#endif

private:
    std::shared_ptr<SceneManager> scene_manager;
#ifndef SERVER_BUILD
    eng::Renderer renderer;   // SDL3 迁移 Step 5：窗口/事件泵/渲染统一走 Renderer
#endif
};
