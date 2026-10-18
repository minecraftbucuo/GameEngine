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
#include "Mario.h"
#include "Goomba.h"
#include "Koopa.h"
#include "PhysicsWorld.h"

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

const char* koopaStateName(KoopaState s) {
    switch (s) {
        case KoopaState::Walking:     return "Walking";
        case KoopaState::ShellIdle:   return "ShellIdle";
        case KoopaState::ShellMoving: return "ShellMoving";
    }
    return "Unknown";
}

// 具体类型的私有状态。这些字段没有统一接口，只能 dynamic_cast 后逐个读；
// 不认识的类保持 Kind::None，JSON 里 typeData 就是 null。
void collectTypeData(GameObject& obj, debug_panel::ObjectView& v) {
    using Kind = debug_panel::ObjectView::TypeData::Kind;

    if (auto* mario = dynamic_cast<Mario*>(&obj)) {
        v.type.kind = Kind::Mario;
        v.type.isBig = mario->getIsBig();
        v.type.isPlayer = mario->getIsPlayer();
        v.type.transforming = mario->isTransforming();

        // 血量在组件里，不在 Mario 自己身上
        if (const auto hp = obj.tryGetComponent<HealthBar>()) {
            v.type.health = hp->getHealth();
        }
        return;
    }

    if (auto* goomba = dynamic_cast<Goomba*>(&obj)) {
        v.type.kind = Kind::Goomba;
        v.type.squashed = goomba->isDead();
        v.type.killedByFireball = goomba->isKilledByFireball();
        return;
    }

    if (auto* koopa = dynamic_cast<Koopa*>(&obj)) {
        v.type.kind = Kind::Koopa;
        const KoopaState st = koopa->getState();
        v.type.stateCode = static_cast<int>(st);
        v.type.stateName = koopaStateName(st);
        v.type.shellMoving = koopa->isShellMoving();
        v.type.killed = koopa->isKilled();
        return;
    }
}

// 物理统计。当前工程 usePhysics 从未被置 true，getPhysicsWorld() 恒为 nullptr，
// 所以正常跑起来 enabled=false、其余全是 0——这是真实状态，不是采集失败。
debug_panel::PhysicsStats collectPhysics(Scene* scene) {
    debug_panel::PhysicsStats st;

    if (scene == nullptr) return st;

    physics::PhysicsWorld* pw = scene->getPhysicsWorld();
    if (pw == nullptr) return st;

    b2World* world = pw->getWorld();
    if (world == nullptr) return st;

    st.enabled = true;

    for (b2Body* body = world->GetBodyList(); body != nullptr; body = body->GetNext()) {
        ++st.bodyCount;

        switch (body->GetType()) {
            case b2_staticBody:    ++st.staticBodies;    break;
            case b2_kinematicBody: ++st.kinematicBodies; break;
            case b2_dynamicBody:   ++st.dynamicBodies;   break;
        }

        if (body->IsAwake()) {
            ++st.awakeBodies;
        } else {
            ++st.sleepingBodies;
        }

        const b2Vec2 pos = body->GetPosition();
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) {
            ++st.nonFiniteBodies;
        }
    }

    for (b2Contact* c = world->GetContactList(); c != nullptr; c = c->GetNext()) {
        ++st.contactCount;
        if (c->IsTouching()) ++st.touchingContacts;
    }

    return st;
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
    collectTypeData(obj, v);

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

    meta.physics = collectPhysics(scene);

    // 客户端列表只在服务端有意义；没有 NetworkManager 就留空
    if (scene != nullptr) {
        if (const NetworkManager* nm = scene->getNetworkManager()) {
            const auto infos = nm->collectClientDebugInfo();
            meta.clients.reserve(infos.size());
            for (const auto& c : infos) {
                debug_panel::ClientView cv;
                cv.address = c.address;
                cv.connected = c.connected;
                cv.verified = c.verified;
                cv.playerId = c.playerId;
                cv.playerClass = c.playerClass;
                cv.lastRecvAgeMs = c.lastRecvAgeMs;
                cv.packetsRecv = c.packetsRecv;
                cv.bytesRecv = c.bytesRecv;
                meta.clients.push_back(std::move(cv));
            }
            meta.unverifiedClients = nm->getUnverifiedCount();
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
