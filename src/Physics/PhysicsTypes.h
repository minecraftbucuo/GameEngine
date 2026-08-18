//
// Created by MINEC on 2026/8/18.
//

#pragma once

#include <box2d/box2d.h>
#include <SFML/System/Vector2.hpp>

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
inline b2Vec2 toMeters(const sf::Vector2f& pixels) {
    return b2Vec2(pixels.x * INV_PPM, pixels.y * INV_PPM);
}

// Box2D 米向量 → SFML 像素向量
inline sf::Vector2f toPixels(const b2Vec2& meters) {
    return sf::Vector2f(meters.x * PPM, meters.y * PPM);
}

// 引擎内部 body 类型枚举，与 b2BodyType 一一对应
enum class BodyType : int {
    Static = b2_staticBody,
    Kinematic = b2_kinematicBody,
    Dynamic = b2_dynamicBody
};

} // namespace physics
