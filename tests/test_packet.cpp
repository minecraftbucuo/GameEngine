//
// eng::Packet 单元测试
//
// 重点之一是「黄金字节」测试：Packet.h 头注释声明线格式逐字节复刻 sf::Packet，
// 这是新旧版本 exe 能交叉联机的前提。这里把每个类型的字节布局钉死，
// 将来任何重构若改动了字节序或长度前缀，测试立刻变红。
//
#include <doctest/doctest.h>

#include "Packet.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> bytesOf(const eng::Packet& p) {
    const auto* b = static_cast<const std::uint8_t*>(p.getData());
    return {b, b + p.getDataSize()};
}

enum class Small : std::uint8_t { A = 1, B = 2 };
enum class Wide : std::int32_t { X = -5, Y = 7 };

} // namespace

// ── 往返 ──────────────────────────────────────────────────────────

TEST_CASE("Packet: 整型往返（含负数与 uint32 高位）") {
    eng::Packet p;
    p << std::int32_t(-123456) << std::uint32_t(4000000000u);
    REQUIRE(p.isValid());

    std::int32_t a = 0;
    std::uint32_t b = 0;
    p >> a >> b;

    CHECK(p.isValid());
    CHECK(a == -123456);
    CHECK(b == 4000000000u);
}

TEST_CASE("Packet: bool 往返") {
    eng::Packet p;
    p << true << false;

    bool t = false;
    bool f = true;
    p >> t >> f;

    CHECK(p.isValid());
    CHECK(t == true);
    CHECK(f == false);
}

TEST_CASE("Packet: float 往返（按位相等，不是近似）") {
    eng::Packet p;
    p << 1.0f << -3.5f << 0.0f;

    float a = 9.0f;
    float b = 9.0f;
    float c = 9.0f;
    p >> a >> b >> c;

    CHECK(p.isValid());
    CHECK(a == 1.0f);
    CHECK(b == -3.5f);
    CHECK(c == 0.0f);
}

TEST_CASE("Packet: string 往返（含空串与内嵌 NUL）") {
    const std::string withNul("ab\0cd", 5);

    eng::Packet p;
    p << std::string("hello") << std::string() << withNul;

    std::string a;
    std::string b = "dirty";
    std::string c;
    p >> a >> b >> c;

    CHECK(p.isValid());
    CHECK(a == "hello");
    CHECK(b.empty());
    CHECK(c == withNul);          // 长度前缀而非 '\0' 结尾，内嵌 NUL 不丢
    CHECK(c.size() == 5);
}

TEST_CASE("Packet: 混合序列往返") {
    eng::Packet p;
    p << std::int32_t(7) << std::string("mario") << 1.5f << true << std::uint32_t(9);

    std::int32_t i = 0;
    std::string s;
    float f = 0.f;
    bool bl = false;
    std::uint32_t u = 0;
    p >> i >> s >> f >> bl >> u;

    CHECK(p.isValid());
    CHECK(i == 7);
    CHECK(s == "mario");
    CHECK(f == 1.5f);
    CHECK(bl == true);
    CHECK(u == 9u);
    CHECK(p.getDataSize() == 4 + 4 + 5 + 4 + 1 + 4);   // 总字节数也可预期
}

// ── 黄金字节：锁死线格式 ──────────────────────────────────────────

TEST_CASE("Packet: 黄金字节 —— int32 走网络字节序（大端）") {
    eng::Packet p;
    p << std::int32_t(1);
    CHECK(bytesOf(p) == std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x01});
}

TEST_CASE("Packet: 黄金字节 —— uint32 大端") {
    eng::Packet p;
    p << std::uint32_t(0x01020304u);
    CHECK(bytesOf(p) == std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04});
}

TEST_CASE("Packet: 黄金字节 —— bool 占 1 字节") {
    eng::Packet p;
    p << true << false;
    CHECK(bytesOf(p) == std::vector<std::uint8_t>{0x01, 0x00});
}

TEST_CASE("Packet: 黄金字节 —— string = uint32 大端长度 + 字节流") {
    eng::Packet p;
    p << std::string("abc");
    CHECK(bytesOf(p) == std::vector<std::uint8_t>{
        0x00, 0x00, 0x00, 0x03, 'a', 'b', 'c'
    });
}

TEST_CASE("Packet: 黄金字节 —— float 原样直拷，不换序") {
    // 头注释声明 float 复刻 sf::Packet 的「不换序」：整数走大端，浮点走原始内存。
    // 所以不能断言固定字节（那会绑死主机字节序），而应断言
    // 「与内存里的原始表示逐字节一致」—— 这才是该契约的可移植表达。
    const float v = 1.0f;

    eng::Packet p;
    p << v;

    std::uint8_t raw[sizeof(float)];
    std::memcpy(raw, &v, sizeof(float));

    REQUIRE(p.getDataSize() == sizeof(float));
    CHECK(std::memcmp(p.getData(), raw, sizeof(float)) == 0);
}

// ── 枚举按底层类型编码 ────────────────────────────────────────────

TEST_CASE("Packet: 枚举按底层类型编码（1 字节 / 4 字节）") {
    eng::Packet p;
    p << Small::B << Wide::X;
    CHECK(p.getDataSize() == 1 + 4);

    Small s{};
    Wide w{};
    p >> s >> w;

    CHECK(p.isValid());
    CHECK(s == Small::B);
    CHECK(w == Wide::X);
}

// ── 边界与失效路径 ────────────────────────────────────────────────

TEST_CASE("Packet: 读越界置 isValid=false 且不写入目标") {
    eng::Packet p;
    p << std::int32_t(42);

    std::int32_t first = 0;
    p >> first;
    CHECK(p.isValid());
    CHECK(first == 42);

    std::int32_t extra = 999;
    p >> extra;
    CHECK_FALSE(p.isValid());
    CHECK(extra == 999);                 // 失败读取不修改目标值
}

TEST_CASE("Packet: 空包读取即失效") {
    eng::Packet p;
    CHECK(p.getDataSize() == 0);
    CHECK(p.isValid());

    std::int32_t v = 1;
    p >> v;
    CHECK_FALSE(p.isValid());
    CHECK(v == 1);
}

TEST_CASE("Packet: 字符串声称长度超出剩余数据 —— 不崩、置失效") {
    // 这正是 TcpClient 组帧那道边界检查依赖的语义：长度前缀不可信时，
    // 必须安全失败而不是越界读。
    eng::Packet p;
    p << std::uint32_t(100);             // 声称后面有 100 字节，实际没有

    std::string s = "unchanged";
    p >> s;

    CHECK_FALSE(p.isValid());
    CHECK(s == "unchanged");
}

TEST_CASE("Packet: clear 复位数据与读取位置") {
    eng::Packet p;
    p << std::int32_t(1);

    std::int32_t v = 0;
    p >> v;
    REQUIRE(p.isValid());

    p.clear();
    CHECK(p.getDataSize() == 0);
    CHECK(p.isValid());

    std::int32_t again = 0;
    p >> again;                          // 复位后再读空包 → 失效
    CHECK_FALSE(p.isValid());
}

TEST_CASE("Packet: append 原始字节后可按帧读取") {
    // 复刻 TcpClient 的用法：先 append 一段 payload，再 >> 出内容
    const std::uint8_t payload[] = {0x00, 0x00, 0x00, 0x07};

    eng::Packet p;
    p.append(payload, sizeof(payload));

    std::uint32_t v = 0;
    p >> v;

    CHECK(p.isValid());
    CHECK(v == 7u);
}
