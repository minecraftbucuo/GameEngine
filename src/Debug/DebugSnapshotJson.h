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

// 碰撞体的真实几何。与对象自身的 w/h 不一定相同：
// Mario 长大后碰撞盒会放大，Koopa 从行走换到龟壳时尺寸也变。
// 只输出左上角 + 宽高（圆也折算成外接矩形），前端画起来统一。
struct CollisionGeom {
    enum class Shape { None, Box, Circle };

    Shape shape = Shape::None;
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float r = 0.f;   // 仅 Circle 有意义
};

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
    CollisionGeom collisionGeom;  // 碰撞体真实几何（可能与 w/h 不同）

    struct TypeData {
        enum class Kind { None, Mario, Goomba, Koopa };

        Kind kind = Kind::None;

        // Mario
        int health = -1;
        bool isBig = false;
        bool isPlayer = false;
        bool transforming = false;

        // Goomba
        bool squashed = false;
        bool killedByFireball = false;

        // Koopa
        int stateCode = 0;
        std::string stateName;
        bool shellMoving = false;
        bool killed = false;
    } type;
};

struct PhysicsStats {
    bool enabled = false;        // false = 场景没启用物理（usePhysics 未开）
    int bodyCount = 0;
    int staticBodies = 0;
    int kinematicBodies = 0;
    int dynamicBodies = 0;
    int awakeBodies = 0;
    int sleepingBodies = 0;
    int contactCount = 0;
    int touchingContacts = 0;
    int nonFiniteBodies = 0;
};

struct ClientView {
    std::string address;
    bool connected = false;
    bool verified = false;
    unsigned int playerId = 0;
    std::string playerClass;
    std::int64_t lastRecvAgeMs = -1;   // -1 = 从未收到过包
    std::uint64_t packetsRecv = 0;
    std::uint64_t bytesRecv = 0;
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

    PhysicsStats physics;
    std::vector<ClientView> clients;
    std::size_t unverifiedClients = 0;
};

inline nlohmann::json typeDataToJson(const ObjectView::TypeData& t) {
    if (t.kind == ObjectView::TypeData::Kind::None) return nullptr;

    nlohmann::json j = nlohmann::json::object();
    switch (t.kind) {
        case ObjectView::TypeData::Kind::Mario:
            j["kind"] = "Mario";
            j["health"] = t.health;
            j["isBig"] = t.isBig;
            j["isPlayer"] = t.isPlayer;
            j["transforming"] = t.transforming;
            break;
        case ObjectView::TypeData::Kind::Goomba:
            j["kind"] = "Goomba";
            j["squashed"] = t.squashed;
            j["killedByFireball"] = t.killedByFireball;
            break;
        case ObjectView::TypeData::Kind::Koopa:
            j["kind"] = "Koopa";
            j["state"] = t.stateName;
            j["stateCode"] = t.stateCode;
            j["shellMoving"] = t.shellMoving;
            j["killed"] = t.killed;
            break;
        case ObjectView::TypeData::Kind::None:
            break;
    }
    return j;
}

inline nlohmann::json collisionGeomToJson(const CollisionGeom& g) {
    if (g.shape == CollisionGeom::Shape::None) return nullptr;

    nlohmann::json j = nlohmann::json::object();
    j["shape"] = g.shape == CollisionGeom::Shape::Box ? "box" : "circle";
    j["x"] = g.x;
    j["y"] = g.y;
    j["w"] = g.w;
    j["h"] = g.h;
    if (g.shape == CollisionGeom::Shape::Circle) j["r"] = g.r;
    return j;
}

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

    j["collisionBox"] = collisionGeomToJson(v.collisionGeom);

    j["typeData"] = typeDataToJson(v.type);

    return j;
}

inline nlohmann::json physicsToJson(const PhysicsStats& p) {
    return nlohmann::json::object({
        {"enabled", p.enabled},
        {"bodyCount", p.bodyCount},
        {"staticBodies", p.staticBodies},
        {"kinematicBodies", p.kinematicBodies},
        {"dynamicBodies", p.dynamicBodies},
        {"awakeBodies", p.awakeBodies},
        {"sleepingBodies", p.sleepingBodies},
        {"contactCount", p.contactCount},
        {"touchingContacts", p.touchingContacts},
        {"nonFiniteBodies", p.nonFiniteBodies},
    });
}

inline nlohmann::json clientsToJson(const SnapshotMeta& m) {
    nlohmann::json list = nlohmann::json::array();

    for (const auto& c : m.clients) {
        nlohmann::json item = nlohmann::json::object();
        item["address"] = c.address;
        item["connected"] = c.connected;
        item["verified"] = c.verified;
        item["playerId"] = c.verified ? nlohmann::json(c.playerId) : nlohmann::json(nullptr);
        item["playerClass"] = c.playerClass;
        item["lastRecvAgeMs"] =
            c.lastRecvAgeMs < 0 ? nlohmann::json(nullptr) : nlohmann::json(c.lastRecvAgeMs);
        item["packetsRecv"] = c.packetsRecv;
        item["bytesRecv"] = c.bytesRecv;
        list.push_back(std::move(item));
    }

    return nlohmann::json::object({
        {"count", m.clients.size()},
        {"unverified", m.unverifiedClients},
        {"list", std::move(list)},
    });
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
    root["physics"] = physicsToJson(m.physics);
    root["clients"] = clientsToJson(m);

    return root;
}

} // namespace debug_panel
