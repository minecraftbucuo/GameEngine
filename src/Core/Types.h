//
// Created by MINEC on 2026/8/21.
//

#pragma once

// 引擎自有数值类型（SDL3 迁移 Step 10：从 SFML 别名换成自研实现）
// 语义与原 SFML 类型逐条对齐：运算符集、类型间隐式转换、字段名全部保持，
// 游戏层代码零改动。Network/ 模块继续直接用 sf:: 类型（迁移范围外）。

#include <cstdint>

namespace eng {

using Uint8 = std::uint8_t;

// ── 二维向量 ────────────────────────────────────────────────────────
// 模板 + 转换构造（Vec2i → Vec2f 等隐式转换，SFML Vector2 同款语义）
template <typename T>
struct Vec2 {
    T x{}, y{};

    Vec2() = default;
    Vec2(T x_, T y_) : x(x_), y(y_) {}
    template <typename U>
    Vec2(const Vec2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

    Vec2& operator+=(const Vec2& r) { x += r.x; y += r.y; return *this; }
    Vec2& operator-=(const Vec2& r) { x -= r.x; y -= r.y; return *this; }
    Vec2& operator*=(T s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(T s) { x /= s; y /= s; return *this; }
};

template <typename T> Vec2<T> operator+(const Vec2<T>& l, const Vec2<T>& r) { return {l.x + r.x, l.y + r.y}; }
template <typename T> Vec2<T> operator-(const Vec2<T>& l, const Vec2<T>& r) { return {l.x - r.x, l.y - r.y}; }
template <typename T> Vec2<T> operator*(const Vec2<T>& l, T s) { return {l.x * s, l.y * s}; }
template <typename T> Vec2<T> operator*(T s, const Vec2<T>& r) { return {r.x * s, r.y * s}; }
template <typename T> Vec2<T> operator/(const Vec2<T>& l, T s) { return {l.x / s, l.y / s}; }
template <typename T> Vec2<T> operator-(const Vec2<T>& v) { return {-v.x, -v.y}; }
template <typename T> bool operator==(const Vec2<T>& l, const Vec2<T>& r) { return l.x == r.x && l.y == r.y; }
template <typename T> bool operator!=(const Vec2<T>& l, const Vec2<T>& r) { return !(l == r); }

using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<unsigned>;

// ── 三维向量 ────────────────────────────────────────────────────────
template <typename T>
struct Vec3 {
    T x{}, y{}, z{};

    Vec3() = default;
    Vec3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
    template <typename U>
    Vec3(const Vec3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

    Vec3& operator+=(const Vec3& r) { x += r.x; y += r.y; z += r.z; return *this; }
    Vec3& operator-=(const Vec3& r) { x -= r.x; y -= r.y; z -= r.z; return *this; }
    Vec3& operator*=(T s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(T s) { x /= s; y /= s; z /= s; return *this; }
};

template <typename T> Vec3<T> operator+(const Vec3<T>& l, const Vec3<T>& r) { return {l.x + r.x, l.y + r.y, l.z + r.z}; }
template <typename T> Vec3<T> operator-(const Vec3<T>& l, const Vec3<T>& r) { return {l.x - r.x, l.y - r.y, l.z - r.z}; }
template <typename T> Vec3<T> operator*(const Vec3<T>& l, T s) { return {l.x * s, l.y * s, l.z * s}; }
template <typename T> Vec3<T> operator*(T s, const Vec3<T>& r) { return {r.x * s, r.y * s, r.z * s}; }
template <typename T> Vec3<T> operator/(const Vec3<T>& l, T s) { return {l.x / s, l.y / s, l.z / s}; }
template <typename T> Vec3<T> operator-(const Vec3<T>& v) { return {-v.x, -v.y, -v.z}; }
template <typename T> bool operator==(const Vec3<T>& l, const Vec3<T>& r) { return l.x == r.x && l.y == r.y && l.z == r.z; }
template <typename T> bool operator!=(const Vec3<T>& l, const Vec3<T>& r) { return !(l == r); }

using Vec3f = Vec3<float>;

// ── 矩形（SFML Rect 语义：left/top/width/height，左闭右开）──────────
template <typename T>
struct Rect {
    T left{}, top{}, width{}, height{};

    Rect() = default;
    Rect(T l, T t, T w, T h) : left(l), top(t), width(w), height(h) {}
    // SFML Rect 同款：位置 + 尺寸两参构造
    Rect(const Vec2<T>& position, const Vec2<T>& size)
        : left(position.x), top(position.y), width(size.x), height(size.y) {}
    template <typename U>
    Rect(const Rect<U>& r)
        : left(static_cast<T>(r.left)), top(static_cast<T>(r.top)),
          width(static_cast<T>(r.width)), height(static_cast<T>(r.height)) {}

    bool contains(T x, T y) const {
        return x >= left && x < left + width && y >= top && y < top + height;
    }
    bool contains(const Vec2<T>& p) const { return contains(p.x, p.y); }
    Vec2<T> getSize() const { return {width, height}; }
};

using IntRect   = Rect<int>;
using FloatRect = Rect<float>;

// ── 颜色（RGBA 各 8bit；常量与 SFML 取值一致）───────────────────────
// 常量声明与定义分离：类内自类型尚不完整无法初始化（SFML 同样类外定义），
// C++17 inline 定义保证头文件 ODR 安全
struct Color {
    Uint8 r{}, g{}, b{}, a{255};

    Color() = default;
    constexpr Color(Uint8 r_, Uint8 g_, Uint8 b_, Uint8 a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Transparent;
};

inline const Color Color::Black{0, 0, 0};
inline const Color Color::White{255, 255, 255};
inline const Color Color::Red{255, 0, 0};
inline const Color Color::Green{0, 255, 0};
inline const Color Color::Blue{0, 0, 255};
inline const Color Color::Yellow{255, 255, 0};
inline const Color Color::Magenta{255, 0, 255};
inline const Color Color::Cyan{0, 255, 255};
inline const Color Color::Transparent{0, 0, 0, 0};

inline bool operator==(const Color& l, const Color& r) { return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a; }
inline bool operator!=(const Color& l, const Color& r) { return !(l == r); }

// ── 时间（微秒内部存储；API 与 SFML Time 对齐）──────────────────────
class Time {
public:
    Time() = default;

    static Time seconds(float s) { return Time(static_cast<int64_t>(s * 1000000.f)); }
    static Time milliseconds(int64_t ms) { return Time(ms * 1000); }
    static Time microseconds(int64_t us) { return Time(us); }

    [[nodiscard]] float asSeconds() const { return static_cast<float>(micro) / 1000000.f; }
    [[nodiscard]] int64_t asMilliseconds() const { return micro / 1000; }
    [[nodiscard]] int64_t asMicroseconds() const { return micro; }

    Time operator+(const Time& r) const { return Time(micro + r.micro); }
    Time operator-(const Time& r) const { return Time(micro - r.micro); }
    Time& operator+=(const Time& r) { micro += r.micro; return *this; }
    Time& operator-=(const Time& r) { micro -= r.micro; return *this; }
    bool operator==(const Time& r) const { return micro == r.micro; }
    bool operator!=(const Time& r) const { return micro != r.micro; }

private:
    explicit Time(int64_t us) : micro(us) {}
    int64_t micro = 0;
};

} // namespace eng
