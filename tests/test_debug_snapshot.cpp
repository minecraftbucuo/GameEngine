//
// 快照 JSON 组装单元测试
//
// 只测 DebugSnapshotJson.h（纯数据 → JSON），不涉及 Scene / GameObject / SDL，
// 因此可以留在「秒级、完全离线」的测试目标里。
//
#include <doctest/doctest.h>

#include "Debug/DebugSnapshotJson.h"

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

TEST_CASE("DebugSnapshotJson: 动态对象字段完整") {
    debug_panel::ObjectView v;
    v.id = 42;
    v.className = "Mario";
    v.tag = "player:";
    v.active = true;
    v.started = true;
    v.destroyed = false;
    v.movable = true;
    v.x = 100.f;
    v.y = 200.f;
    v.w = 48.f;
    v.h = 64.f;
    v.vx = -10.f;
    v.vy = 5.f;
    v.cx = 124.f;
    v.cy = 232.f;
    v.rotation = 45.f;
    v.finite = true;
    v.hasNetworkId = true;
    v.networkId = 42;
    v.hasMove = true;
    v.hasGravity = true;
    v.hasHealthBar = true;
    v.collision = "BoxCollision";
    v.collisionHandle = "BoxCollisionHandle";

    const auto j = debug_panel::objectViewToJson(v);

    CHECK(j["id"].get<unsigned>() == 42);
    CHECK(j["class"] == "Mario");
    CHECK(j["tag"] == "player:");
    CHECK(j["active"] == true);
    CHECK(j["started"] == true);
    CHECK(j["destroyed"] == false);
    CHECK(j["movable"] == true);

    CHECK(j["x"].get<float>() == doctest::Approx(100.f));
    CHECK(j["y"].get<float>() == doctest::Approx(200.f));
    CHECK(j["w"].get<float>() == doctest::Approx(48.f));
    CHECK(j["h"].get<float>() == doctest::Approx(64.f));
    CHECK(j["vx"].get<float>() == doctest::Approx(-10.f));
    CHECK(j["vy"].get<float>() == doctest::Approx(5.f));
    CHECK(j["cx"].get<float>() == doctest::Approx(124.f));
    CHECK(j["cy"].get<float>() == doctest::Approx(232.f));
    CHECK(j["rotation"].get<float>() == doctest::Approx(45.f));
    CHECK(j["finite"] == true);

    CHECK(j["networkId"].get<unsigned>() == 42);
    CHECK(j["components"]["Move"] == true);
    CHECK(j["components"]["Gravity"] == true);
    CHECK(j["components"]["HealthBar"] == true);
    CHECK(j["components"]["Collision"] == "BoxCollision");
    CHECK(j["components"]["CollisionHandle"] == "BoxCollisionHandle");
    CHECK(j["typeData"].is_null());
}

TEST_CASE("DebugSnapshotJson: 静态对象 networkId 为 null") {
    debug_panel::ObjectView v;
    v.className = "Brick";
    v.hasNetworkId = false;

    const auto j = debug_panel::objectViewToJson(v);

    CHECK(j["networkId"].is_null());
}

TEST_CASE("DebugSnapshotJson: 没有碰撞组件时 Collision 为 null 而不是 false") {
    // 前端要能区分「没有这个组件」和「有这个组件但形状未知」
    debug_panel::ObjectView v;
    v.collision.clear();
    v.collisionHandle.clear();

    const auto j = debug_panel::objectViewToJson(v);

    CHECK(j["components"]["Collision"].is_null());
    CHECK(j["components"]["CollisionHandle"].is_null());
    CHECK(j["components"]["Move"] == false);
}

TEST_CASE("DebugSnapshotJson: 非有限数值显式标记 finite=false") {
    debug_panel::ObjectView v;
    v.x = std::numeric_limits<float>::quiet_NaN();
    v.finite = false;

    const auto j = debug_panel::objectViewToJson(v);

    CHECK(j["finite"] == false);
}

TEST_CASE("DebugSnapshotJson: 顶层结构与 meta 字段") {
    debug_panel::SnapshotMeta m;
    m.seq = 9;
    m.frame = 100;
    m.uptimeSec = 3.5;
    m.snapshotHz = 10;
    m.scene = "SuperMarioSceneMultiplayer";
    m.truncated = true;
    m.nonFiniteObjects = 2;

    std::vector<debug_panel::ObjectView> objs(3);
    objs[0].className = "Mario";
    objs[1].className = "Goomba";
    objs[2].className = "Brick";

    const auto j = debug_panel::snapshotToJson(m, objs);

    CHECK(j["meta"]["schemaVersion"] == 1);
    CHECK(j["meta"]["seq"].get<std::uint64_t>() == 9);
    CHECK(j["meta"]["frame"].get<std::uint64_t>() == 100);
    CHECK(j["meta"]["snapshotHz"] == 10);
    CHECK(j["meta"]["scene"] == "SuperMarioSceneMultiplayer");
    CHECK(j["meta"]["truncated"] == true);
    CHECK(j["meta"]["nonFiniteObjects"] == 2);

    // objectCount 由 objects.size() 推导，不接受外部传入的陈旧值
    CHECK(j["meta"]["objectCount"].get<std::size_t>() == 3);

    REQUIRE(j["objects"].is_array());
    CHECK(j["objects"].size() == 3);
    CHECK(j["objects"][0]["class"] == "Mario");
    CHECK(j["objects"][2]["class"] == "Brick");

    CHECK(j["physics"]["enabled"] == false);
    CHECK(j["clients"]["count"] == 0);
    CHECK(j["clients"]["unverified"] == 0);
    CHECK(j["clients"]["list"].is_array());
}

TEST_CASE("DebugSnapshotJson: 空对象列表产出合法结构") {
    debug_panel::SnapshotMeta m;

    const auto j = debug_panel::snapshotToJson(m, {});

    CHECK(j["meta"]["objectCount"].get<std::size_t>() == 0);
    CHECK(j["objects"].is_array());
    CHECK(j["objects"].empty());
    CHECK_NOTHROW(j.dump());
}

TEST_CASE("DebugSnapshotJson: typeData 只输出对应类型的字段") {
    using Kind = debug_panel::ObjectView::TypeData::Kind;

    SUBCASE("Mario") {
        debug_panel::ObjectView v;
        v.className = "Mario";
        v.type.kind = Kind::Mario;
        v.type.health = 2;
        v.type.isBig = true;
        v.type.isPlayer = true;
        v.type.transforming = true;
        // 其他类型的字段即使被误填也不该出现
        v.type.squashed = true;
        v.type.stateName = "Walking";

        const auto t = debug_panel::objectViewToJson(v)["typeData"];

        CHECK(t["kind"] == "Mario");
        CHECK(t["health"] == 2);
        CHECK(t["isBig"] == true);
        CHECK(t["isPlayer"] == true);
        CHECK(t["transforming"] == true);
        CHECK_FALSE(t.contains("squashed"));
        CHECK_FALSE(t.contains("state"));
    }

    SUBCASE("Goomba") {
        debug_panel::ObjectView v;
        v.className = "Goomba";
        v.type.kind = Kind::Goomba;
        v.type.squashed = true;
        v.type.killedByFireball = true;

        const auto t = debug_panel::objectViewToJson(v)["typeData"];

        CHECK(t["kind"] == "Goomba");
        CHECK(t["squashed"] == true);
        CHECK(t["killedByFireball"] == true);
        CHECK_FALSE(t.contains("health"));
        CHECK_FALSE(t.contains("state"));
    }

    SUBCASE("Koopa") {
        debug_panel::ObjectView v;
        v.className = "Koopa";
        v.type.kind = Kind::Koopa;
        v.type.stateCode = 2;
        v.type.stateName = "ShellMoving";
        v.type.shellMoving = true;
        v.type.killed = false;

        const auto t = debug_panel::objectViewToJson(v)["typeData"];

        CHECK(t["kind"] == "Koopa");
        CHECK(t["state"] == "ShellMoving");
        CHECK(t["stateCode"] == 2);
        CHECK(t["shellMoving"] == true);
        CHECK(t["killed"] == false);
        CHECK_FALSE(t.contains("squashed"));
    }

    SUBCASE("未知类型为 null") {
        debug_panel::ObjectView v;
        v.className = "Brick";

        CHECK(debug_panel::objectViewToJson(v)["typeData"].is_null());
    }
}

TEST_CASE("DebugSnapshotJson: physics 字段完整") {
    debug_panel::PhysicsStats p;
    p.enabled = true;
    p.bodyCount = 12;
    p.staticBodies = 7;
    p.kinematicBodies = 1;
    p.dynamicBodies = 4;
    p.awakeBodies = 3;
    p.sleepingBodies = 9;
    p.contactCount = 5;
    p.touchingContacts = 2;
    p.nonFiniteBodies = 1;

    const auto j = debug_panel::physicsToJson(p);

    CHECK(j["enabled"] == true);
    CHECK(j["bodyCount"] == 12);
    CHECK(j["staticBodies"] == 7);
    CHECK(j["kinematicBodies"] == 1);
    CHECK(j["dynamicBodies"] == 4);
    CHECK(j["awakeBodies"] == 3);
    CHECK(j["sleepingBodies"] == 9);
    CHECK(j["contactCount"] == 5);
    CHECK(j["touchingContacts"] == 2);
    CHECK(j["nonFiniteBodies"] == 1);
}

TEST_CASE("DebugSnapshotJson: 客户端列表与未验证计数") {
    debug_panel::SnapshotMeta m;
    m.unverifiedClients = 2;

    debug_panel::ClientView a;
    a.address = "127.0.0.1:50001";
    a.connected = true;
    a.verified = true;
    a.playerId = 77;
    a.playerClass = "Mario";
    a.lastRecvAgeMs = 12;
    a.packetsRecv = 1000;
    a.bytesRecv = 64000;
    m.clients.push_back(a);

    // 未验证的连接：没有 playerId，也还没收到过包
    debug_panel::ClientView b;
    b.address = "192.168.1.9:50002";
    b.connected = true;
    b.verified = false;
    b.lastRecvAgeMs = -1;
    m.clients.push_back(b);

    const auto j = debug_panel::clientsToJson(m);

    CHECK(j["count"] == 2);
    CHECK(j["unverified"] == 2);
    REQUIRE(j["list"].is_array());
    REQUIRE(j["list"].size() == 2);

    CHECK(j["list"][0]["address"] == "127.0.0.1:50001");
    CHECK(j["list"][0]["verified"] == true);
    CHECK(j["list"][0]["playerId"].get<unsigned>() == 77);
    CHECK(j["list"][0]["playerClass"] == "Mario");
    CHECK(j["list"][0]["lastRecvAgeMs"].get<std::int64_t>() == 12);
    CHECK(j["list"][0]["packetsRecv"].get<std::uint64_t>() == 1000);

    // 未验证客户端不暴露 playerId，未收包时 lastRecvAgeMs 为 null（不是 -1）
    CHECK(j["list"][1]["verified"] == false);
    CHECK(j["list"][1]["playerId"].is_null());
    CHECK(j["list"][1]["lastRecvAgeMs"].is_null());
}
