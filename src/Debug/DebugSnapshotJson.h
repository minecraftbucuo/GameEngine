//
// 快照 JSON 组装（纯数据 → JSON，不依赖 Scene / GameObject / SDL）
//
// 拆出来的目的：让「JSON 字段是否完整、类型是否正确」能进单元测试。
// DebugSnapshot.cpp 依赖 Scene.h → NetworkManager.h → SDL_net，测试目标刻意不链 SDL，
// 所以那部分无法直接测试；这里只接收已经采集好的纯值。
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace debug_panel {

// 单个对象的快照视图：字段全部是采集好的值，不含任何指针
struct ObjectView {
    unsigned int id = 0;
    std::string className;
    std::string tag;

    bool active = true;
    bool started = false;
    bool destroyed = false;
    bool movable = true;

    float x = 0.f, y = 0.f;      // 左上角
    float w = 0.f, h = 0.f;
    float vx = 0.f, vy = 0.f;
    float cx = 0.f, cy = 0.f;    // 中心点（getCenter 可能被子类覆写）
    float rotation = 0.f;

    bool finite = true;          // 任一数值非有限则 false
    bool hasNetworkId = false;   // 只有实现 ISerializable 的动态对象才有
    unsigned int networkId = 0;

    bool hasMove = false;
    bool hasGravity = false;
    bool hasHealthBar = false;
    std::string collision;        // 空 = 没有碰撞组件；否则是具体形状名
    std::string collisionHandle;  // 空 = 没有碰撞句柄
};

struct SnapshotMeta {
    std::uint64_t seq = 0;
    std::uint64_t frame = 0;
    double uptimeSec = 0.0;
    float dtMs = 0.f;
    float avgFrameMs = 0.f;
    std::uint64_t buildCostUs = 0;
    int snapshotHz = 10;
    std::string scene;
    std::size_t objectCount = 0;
    bool truncated = false;
    int nonFiniteObjects = 0;
};

inline nlohmann::json objectViewToJson(const ObjectView& v) {
    nlohmann::json j = nlohmann::json::object();

    j["id"] = v.id;
    j["class"] = v.className;
    j["tag"] = v.tag;
    j["active"] = v.active;
    j["started"] = v.started;
    j["destroyed"] = v.destroyed;
    j["movable"] = v.movable;

    j["x"] = v.x;
    j["y"] = v.y;
    j["w"] = v.w;
    j["h"] = v.h;
    j["vx"] = v.vx;
    j["vy"] = v.vy;
    j["cx"] = v.cx;
    j["cy"] = v.cy;
    j["rotation"] = v.rotation;
    j["finite"] = v.finite;

    if (v.hasNetworkId) {
        j["networkId"] = v.networkId;
    } else {
        j["networkId"] = nullptr;
    }

    nlohmann::json c = nlohmann::json::object();
    c["Move"] = v.hasMove;
    c["Gravity"] = v.hasGravity;
    c["HealthBar"] = v.hasHealthBar;
    c["Collision"] = v.collision.empty() ? nlohmann::json(nullptr) : nlohmann::json(v.collision);
    c["CollisionHandle"] =
        v.collisionHandle.empty() ? nlohmann::json(nullptr) : nlohmann::json(v.collisionHandle);
    j["components"] = std::move(c);

    j["typeData"] = nullptr;   // B5 接入类型特有字段

    return j;
}

inline nlohmann::json snapshotToJson(const SnapshotMeta& m,
                                     const std::vector<ObjectView>& objects) {
    nlohmann::json meta = nlohmann::json::object();
    meta["schemaVersion"] = 1;
    meta["seq"] = m.seq;
    meta["frame"] = m.frame;
    meta["uptimeSec"] = m.uptimeSec;
    meta["dtMs"] = m.dtMs;
    meta["avgFrameMs"] = m.avgFrameMs;
    meta["snapshotHz"] = m.snapshotHz;
    meta["buildCostUs"] = m.buildCostUs;
    meta["scene"] = m.scene;
    meta["objectCount"] = objects.size();
    meta["truncated"] = m.truncated;
    meta["nonFiniteObjects"] = m.nonFiniteObjects;

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& v : objects) {
        arr.push_back(objectViewToJson(v));
    }

    nlohmann::json root = nlohmann::json::object();
    root["meta"] = std::move(meta);
    root["objects"] = std::move(arr);

    // 物理与客户端列表在 B5 接入，先给稳定结构，前端不必做字段存在性判断
    root["physics"] = nlohmann::json::object({{"enabled", false}});
    root["clients"] = nlohmann::json::object({
        {"count", 0},
        {"unverified", 0},
        {"list", nlohmann::json::array()},
    });

    return root;
}

} // namespace debug_panel
