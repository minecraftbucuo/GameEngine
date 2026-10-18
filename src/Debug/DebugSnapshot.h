//
// 调试面板快照构建
//
// 只在主线程调用：遍历场景对象、读字段、产出 JSON 字符串。
// HTTP 线程拿到的是构建完成的不可变字符串，永远不接触 GameObject / Scene。
//
#pragma once
#ifdef SERVER_BUILD

#include <cstdint>
#include <string>

class Scene;

struct DebugSnapshotContext {
    std::uint64_t seq = 0;              // 单调递增，前端据此判断是否拿到新帧
    std::uint64_t frame = 0;            // 主循环累计帧数
    double uptimeSec = 0.0;
    float dtMs = 0.0f;                  // 本帧耗时
    float avgFrameMs = 0.0f;            // 指数平均帧耗时
    std::uint64_t lastBuildCostUs = 0;  // 上一次快照构建耗时（本次无法自引用）
    int snapshotHz = 10;
    int maxObjects = 2000;
};

// 返回紧凑 JSON。costUs 非空时回填本次构建耗时（含 dump）。
std::string buildDebugSnapshot(Scene* scene,
                               const DebugSnapshotContext& ctx,
                               std::uint64_t* costUs);

#endif // SERVER_BUILD
