//
// Created by MINEC on 2026/8/23.
//
#pragma once

// 引擎自有数值类型（SDL3 迁移 Step 1）
// 当前阶段：别名到 SFML 类型，语义零变化；
// SDL3 落地时（Step 10）本文件换成自研 struct，运算符 API 保持一致，
// 届时只有 Renderer/AssetManager 等实现文件接触第三方 API。
// 使用范围：除 src/Network/ 外的全部源码（网络模块继续直接用 sf:: 类型）。
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>

namespace eng {
    using Vec2f = sf::Vector2f;
    using Vec2i = sf::Vector2i;
    using Vec2u = sf::Vector2u;
    using Vec3f = sf::Vector3f;
    using Time  = sf::Time;
    using Color = sf::Color;
    using Uint8 = sf::Uint8;
    using IntRect   = sf::IntRect;
    using FloatRect = sf::FloatRect;
}
