//
// 调试面板快照构建实现
//
// 客户端构建也会用 GLOB 把这个文件编进来，整文件用 SERVER_BUILD 守卫。
//
// 职责拆分：
//   本文件       —— 遍历场景、读字段、填 ObjectView（依赖 Scene/GameObject）
//   DebugSnapshotJson.h —— ObjectView → JSON（纯数据，可单元测试）
//
#ifdef SERVER_BUILD

#include "DebugSnapshot.h"

#include "DebugSnapshotJson.h"

#include "Scene.h"
#include "GameObject.h"
#include "ISerializable.h"

#include "Components/MoveComponent.h"
#include "Components/GravityComponent.h"
#include "Components/HealthBar.h"
#include "Components/Collisions/Collision.h"
#include "Components/Collisions/BoxCollision.h"
#include "Components/Collisions/CircleCollision.h"
#include "Components/CollisionHandles/CollisionHandle.h"
#include "Components/CollisionHandles/BoxCollisionHandle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace {

bool allFinite(std::initializer_list<float> values) {
    for (const float v : values) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

// 组件探测。key 用的是 addComponent 时的「接口类型」，不是具体实现类型：
// 源码里普遍是 addComponent<Collision, BoxCollision>()，map 的 key 是 typeid(Collision)，
// 直接探测 BoxCollision 永远返回 nullptr。所以先探 Collision，再 dynamic_cast 判形状。
void collectComponents(GameObject& obj, debug_panel::ObjectView& v) {
    v.hasMove = obj.tryGetComponent<MoveComponent>() != nullptr;
    v.hasGravity = obj.tryGetComponent<GravityComponent>() != nullptr;
    v.hasHealthBar = obj.tryGetComponent<HealthBar>() != nullptr;

    if (const auto col = obj.tryGetComponent<Collision>()) {
        if (dynamic_cast<BoxCollision*>(col.get()) != nullptr) {
            v.collision = "BoxCollision";
        } else if (dynamic_cast<CircleCollision*>(col.get()) != nullptr) {
            v.collision = "CircleCollision";
        } else {
            v.collision = "unknown";
        }
    }

    if (const auto handle = obj.tryGetComponent<CollisionHandle>()) {
        v.collisionHandle = dynamic_cast<BoxCollisionHandle*>(handle.get()) != nullptr
                                ? "BoxCollisionHandle"
                                : "unknown";
    }
}

debug_panel::ObjectView collectObject(GameObject& obj) {
    debug_panel::ObjectView v;

    v.id = obj.getId();
    v.className = obj.getClassName();
    v.tag = obj.getTag();
    v.active = obj.isActive();
    v.started = obj.hasStarted();
    v.destroyed = obj.isDestroy();
    v.movable = obj.getMoveAble();

    const eng::Vec2f& p = obj.getPosition();
    const eng::Vec2f& s = obj.getSize();
    const eng::Vec2f& sp = obj.getSpeed();
    const eng::Vec2f center = obj.getCenter();

    v.x = p.x;
    v.y = p.y;
    v.w = s.x;
    v.h = s.y;
    v.vx = sp.x;
    v.vy = sp.y;
    v.cx = center.x;
    v.cy = center.y;
    v.rotation = obj.getRotation();

    // dump() 会把 NaN/Inf 静默写成 null，看起来像字段丢失；
    // 显式标记出来，前端才能把"坐标炸了"和"字段没读到"区分开。
    v.finite = allFinite({v.x, v.y, v.w, v.h, v.vx, v.vy, v.cx, v.cy, v.rotation});

    // 只有动态对象实现 ISerializable；静态砖块/地面/箱子没有网络 id
    if (auto* net = dynamic_cast<ISerializable*>(&obj)) {
        v.hasNetworkId = true;
        v.networkId = net->getNetworkId();
    }

    collectComponents(obj, v);

    return v;
}

} // namespace

std::string buildDebugSnapshot(Scene* scene, const DebugSnapshotContext& ctx,
                               std::uint64_t* costUs) {
    const auto t0 = std::chrono::steady_clock::now();

    debug_panel::SnapshotMeta meta;
    meta.seq = ctx.seq;
    meta.frame = ctx.frame;
    meta.uptimeSec = ctx.uptimeSec;
    meta.dtMs = ctx.dtMs;
    meta.avgFrameMs = ctx.avgFrameMs;
    meta.snapshotHz = ctx.snapshotHz;
    meta.buildCostUs = ctx.lastBuildCostUs;
    meta.scene = scene != nullptr ? scene->getSceneName() : "";

    std::vector<debug_panel::ObjectView> views;

    if (scene != nullptr) {
        auto& list = scene->getGameObjects();
        const std::size_t limit = static_cast<std::size_t>(std::max(ctx.maxObjects, 0));
        const std::size_t count = std::min(list.size(), limit);
        meta.truncated = list.size() > count;

        views.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (!list[i]) continue;   // 理论上不该有 null，防御一下
            views.push_back(collectObject(*list[i]));
            if (!views.back().finite) ++meta.nonFiniteObjects;
        }
    }

    std::string out = debug_panel::snapshotToJson(meta, views).dump();

    if (costUs != nullptr) {
        const auto t1 = std::chrono::steady_clock::now();
        *costUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }

    return out;
}

#endif // SERVER_BUILD
