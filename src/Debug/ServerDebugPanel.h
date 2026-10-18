//
// 服务端 Web 调试面板
//
// 独立 HTTP 线程 + 主线程快照发布。设计见 docs/server-debug-panel-plan.md。
//
// 头文件刻意不 include httplib（PIMPL 隔离）：
// Logger.h 无条件 include <windows.h>，而 httplib 内部先 include <winsock2.h>，
// 同一 TU 里顺序反了会触发 struct timeval / fd_set 重定义。所有 httplib 细节
// 都封在 .cpp 里，且 .cpp 的第一个 include 必须是 httplib。
//
#pragma once
#ifdef SERVER_BUILD

#include <memory>

#include "ConfigManager.h"

class Scene;

class ServerDebugPanel {
public:
    ServerDebugPanel();
    ~ServerDebugPanel();

    ServerDebugPanel(const ServerDebugPanel&) = delete;
    ServerDebugPanel& operator=(const ServerDebugPanel&) = delete;

    // 主线程调用：解析 token、绑定端口、起 HTTP 线程。
    // 绑定失败返回 false（端口占用等），调用方应继续跑游戏服务，不要中断。
    bool start(const ConfigManager::DebugPanelConfig& cfg);

    // 停止 HTTP 线程。幂等。
    void stop();

    [[nodiscard]] bool isRunning() const;

    // 主线程每帧调用：B2 阶段只累计帧数，B3 起在这里限频构建快照。
    void onFrame(Scene* scene, float dtSec);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // SERVER_BUILD
