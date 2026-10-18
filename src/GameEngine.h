//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <memory>
#include <chrono>
#include "SceneManager.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"
#endif

#ifdef SERVER_BUILD
// 必须拿到完整定义：GameEngine() = default 是 inline 的，编译器要为它生成
// 异常清理路径，需要能析构 unique_ptr<ServerDebugPanel> 的成员。
// ServerDebugPanel.h 本身不 include httplib（PIMPL 隔离），带进来很轻。
#include "Debug/ServerDebugPanel.h"
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
#ifdef SERVER_BUILD
    // 声明在 scene_manager 之后：析构时先停面板线程，再销毁场景，
    // 避免 HTTP 线程碰到已释放的对象。
    std::unique_ptr<ServerDebugPanel> debug_panel;
#endif
#ifndef SERVER_BUILD
    eng::Renderer renderer;   // SDL3 迁移 Step 5：窗口/事件泵/渲染统一走 Renderer
    bool frameStep();         // WASM 移植 Step 3：单帧迭代，返回 false 表示退出主循环
    std::chrono::steady_clock::time_point lastFrameTime{};   // 上一帧时刻（dt 计算基准）
#endif
};
