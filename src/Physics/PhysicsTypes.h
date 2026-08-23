//
// Created by MINEC on 2026/8/18.
//

#pragma once

#include <box2d/box2d.h>
#include "Core/Types.h"

namespace physics {

// 像素/米：1 米 = 64 像素（与 CONFIG.game.defaultBlockSize 一致）
inline constexpr float PPM = 64.0f;
inline constexpr float INV_PPM = 1.0f / PPM;

// 像素 → 米
inline float toMeters(float pixels) {
    return pixels * INV_PPM;
}

// 米 → 像素
inline float toPixels(float meters) {
    return meters * PPM;
}

// SFML 像素向量 → Box2D 米向量
inline b2Vec2 toMeters(const eng::Vec2f& pixels) {
    return b2Vec2(pixels.x * INV_PPM, pixels.y * INV_PPM);
}

// Box2D 米向量 → SFML 像素向量
inline eng::Vec2f toPixels(const b2Vec2& meters) {
    return eng::Vec2f(meters.x * PPM, meters.y * PPM);
}

// 引擎内部 body 类型枚举，与 b2BodyType 一一对应
enum class BodyType : int {
    Static = b2_staticBody,
    Kinematic = b2_kinematicBody,
    Dynamic = b2_dynamicBody
};

// 碰撞分组 categoryBits（每个分组占一位，可组合）
namespace Category {
    inline constexpr uint16 Player    = 0x0001;
    inline constexpr uint16 Enemy     = 0x0002;
    inline constexpr uint16 Ground    = 0x0004;
    inline constexpr uint16 Brick     = 0x0008;
    inline constexpr uint16 Projectile= 0x0010;
    inline constexpr uint16 Trigger   = 0x0020;
    inline constexpr uint16 All       = 0xFFFF;
    inline constexpr uint16 None      = 0x0000;
} // namespace Category

} // namespace physics
