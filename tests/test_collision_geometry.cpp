//
// 碰撞几何单元测试（BoxCollision / CircleCollision）
//
// 为什么测这个：碰撞判定是玩法核心——踩踏、顶砖、受伤、吃蘑菇全都建立在
// 它之上，改一个符号或边界条件会同时影响一整片逻辑，但它此前没有任何测试。
//
// 这里钉住三类容易出错的语义：
//   1. 边界：相切（距离恰等于半径和 / 矩形边界恰好贴合）算不算碰撞；
//   2. 对称：Box↔Circle 通过虚函数互相委托（checkCollision 转发到对方的
//      checkCollisionWithXxx），这条双重委托链必须满足 a.check(b) == b.check(a)；
//   3. 坐标换算：offset 与 setCollisionPosition / getCollisionPosition 的语义，
//      其中 CircleCollision 对这两个函数有覆写，与基类行为不同。
//
// 纯数学，不依赖 SDL：碰撞源文件在测试目标里以 SERVER_BUILD 编译，
// render() 被 #ifndef 排除，故无需链接渲染后端（详见 tests/CMakeLists.txt）。
//
#include <doctest/doctest.h>

#include "Components/Collisions/BoxCollision.h"
#include "Components/Collisions/CircleCollision.h"

// ── 矩形 vs 矩形 ──────────────────────────────────────────────────

TEST_CASE("BoxCollision: 重叠与分离") {
    const BoxCollision a(0.f, 0.f, 100.f, 100.f);

    SUBCASE("完全重叠") {
        const BoxCollision b(0.f, 0.f, 100.f, 100.f);
        CHECK(a.checkCollisionWithBox(b));
        CHECK(b.checkCollisionWithBox(a));
    }
    SUBCASE("部分重叠") {
        const BoxCollision b(50.f, 50.f, 100.f, 100.f);
        CHECK(a.checkCollisionWithBox(b));
    }
    SUBCASE("X 轴完全分离") {
        const BoxCollision b(200.f, 0.f, 100.f, 100.f);
        CHECK_FALSE(a.checkCollisionWithBox(b));
    }
    SUBCASE("仅 X 分离、Y 重叠") {
        const BoxCollision b(200.f, 50.f, 100.f, 100.f);
        CHECK_FALSE(a.checkCollisionWithBox(b));
    }
}

TEST_CASE("BoxCollision: 相切不算碰撞（边界语义）") {
    const BoxCollision a(0.f, 0.f, 100.f, 100.f);

    SUBCASE("右边界恰好贴合") {
        const BoxCollision b(100.f, 0.f, 100.f, 100.f);
        CHECK_FALSE(a.checkCollisionWithBox(b));
    }
    SUBCASE("右下角恰好贴合") {
        const BoxCollision b(100.f, 100.f, 100.f, 100.f);
        CHECK_FALSE(a.checkCollisionWithBox(b));
    }
    SUBCASE("重叠 1 像素即算碰撞") {
        const BoxCollision b(99.f, 0.f, 100.f, 100.f);
        CHECK(a.checkCollisionWithBox(b));
    }
}

// ── 圆 vs 圆 ──────────────────────────────────────────────────────

TEST_CASE("CircleCollision: 重叠与分离") {
    const CircleCollision a(0.f, 0.f, 10.f);

    SUBCASE("重叠") {
        const CircleCollision b(5.f, 0.f, 10.f);
        CHECK(a.checkCollisionWithCircle(b));
    }
    SUBCASE("同心") {
        const CircleCollision b(0.f, 0.f, 10.f);
        CHECK(a.checkCollisionWithCircle(b));
    }
    SUBCASE("分离") {
        const CircleCollision b(30.f, 0.f, 10.f);
        CHECK_FALSE(a.checkCollisionWithCircle(b));
    }
    SUBCASE("外切不算碰撞") {
        const CircleCollision b(20.f, 0.f, 10.f);
        CHECK_FALSE(a.checkCollisionWithCircle(b));
    }
    SUBCASE("斜向用欧氏距离，不是曼哈顿距离") {
        // (14,14) 到原点欧氏距离 ≈19.8 < 20（相交）；曼哈顿距离 28 > 20
        // 若实现误用 |dx|+|dy| 会返回 false，本用例即可暴露。
        const CircleCollision b(14.f, 14.f, 10.f);
        CHECK(a.checkCollisionWithCircle(b));
    }
}

// ── 矩形 vs 圆 ────────────────────────────────────────────────────

TEST_CASE("BoxCollision: 矩形与圆（最近点算法）") {
    const BoxCollision box(0.f, 0.f, 100.f, 100.f);

    SUBCASE("圆心在矩形内") {
        const CircleCollision c(50.f, 50.f, 10.f);
        CHECK(box.checkCollisionWithCircle(c));
    }
    SUBCASE("圆心在边外、距离小于半径") {
        const CircleCollision c(105.f, 50.f, 10.f);
        CHECK(box.checkCollisionWithCircle(c));
    }
    SUBCASE("圆心在边外、距离大于半径") {
        const CircleCollision c(120.f, 50.f, 10.f);
        CHECK_FALSE(box.checkCollisionWithCircle(c));
    }
    SUBCASE("圆心在角外侧、斜向距离小于半径") {
        // 最近点是角 (100,100)，距离 sqrt(5²+5²)≈7.07 < 10
        const CircleCollision c(105.f, 105.f, 10.f);
        CHECK(box.checkCollisionWithCircle(c));
    }
    SUBCASE("圆心在角外侧、斜向距离大于半径") {
        // 距离 sqrt(10²+10²)≈14.14 > 10
        const CircleCollision c(110.f, 110.f, 10.f);
        CHECK_FALSE(box.checkCollisionWithCircle(c));
    }
}

// ── 对称性：双重虚委托链 ──────────────────────────────────────────

TEST_CASE("碰撞判定对称：a.checkCollision(b) == b.checkCollision(a)") {
    SUBCASE("矩形-矩形") {
        const BoxCollision a(0.f, 0.f, 100.f, 100.f);
        const BoxCollision b(80.f, 80.f, 100.f, 100.f);
        CHECK(a.checkCollision(b));
        CHECK(a.checkCollision(b) == b.checkCollision(a));
    }
    SUBCASE("圆-圆") {
        const CircleCollision a(0.f, 0.f, 10.f);
        const CircleCollision b(15.f, 0.f, 10.f);
        CHECK(a.checkCollision(b));
        CHECK(a.checkCollision(b) == b.checkCollision(a));
    }
    SUBCASE("矩形-圆（跨类型委托）") {
        const BoxCollision box(0.f, 0.f, 100.f, 100.f);
        const CircleCollision circle(105.f, 50.f, 10.f);
        CHECK(box.checkCollision(circle));
        CHECK(box.checkCollision(circle) == circle.checkCollision(box));
    }
}

// ── offset 与坐标换算 ─────────────────────────────────────────────

TEST_CASE("offset 参与碰撞坐标计算") {
    BoxCollision a(0.f, 0.f, 100.f, 100.f);
    const BoxCollision b(150.f, 0.f, 100.f, 100.f);

    CHECK_FALSE(a.checkCollisionWithBox(b));

    // 加 offset 后 a 右移到 [60,160]，与 b 的 [150,250] 相交
    a.setOffset(eng::Vec2f(60.f, 0.f));
    CHECK(a.getPosX() == doctest::Approx(60.f));
    CHECK(a.checkCollisionWithBox(b));
}

TEST_CASE("setCollisionPosition：基类与 CircleCollision 语义不同") {
    SUBCASE("BoxCollision 走基类：position = p - offset") {
        BoxCollision box(0.f, 0.f, 100.f, 100.f);
        box.setCollisionPosition(eng::Vec2f(10.f, 20.f));
        CHECK(box.getPosX() == doctest::Approx(10.f));
        CHECK(box.getPosY() == doctest::Approx(20.f));
    }
    SUBCASE("CircleCollision 覆写：position = p - offset + (r, r)") {
        CircleCollision c(0.f, 0.f, 5.f);
        c.setCollisionPosition(eng::Vec2f(10.f, 20.f));
        // 圆心 = position + offset = (10+5, 20+5)
        CHECK(c.getPos().x == doctest::Approx(15.f));
        CHECK(c.getPos().y == doctest::Approx(25.f));
    }
}

TEST_CASE("getCollisionPosition：圆返回外接矩形左上角") {
    SUBCASE("基类 = position + offset") {
        const BoxCollision box(3.f, 4.f, 100.f, 100.f);
        const eng::Vec2f p = box.getCollisionPosition();
        CHECK(p.x == doctest::Approx(3.f));
        CHECK(p.y == doctest::Approx(4.f));
    }
    SUBCASE("圆 = position + offset - (r, r)") {
        const CircleCollision c(10.f, 20.f, 5.f);
        const eng::Vec2f p = c.getCollisionPosition();
        CHECK(p.x == doctest::Approx(5.f));
        CHECK(p.y == doctest::Approx(15.f));
    }
}
