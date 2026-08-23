//
// Created by MINEC on 2026/8/23.
//
#pragma once

#include <cstdint>
#include <functional>

// 引擎资源句柄（SDL3 迁移 Step 4）
// 句柄只是 AssetManager 内部资源表的 id，游戏层永远不接触 sf::Texture / SDL_Texture。
// 0 为无效值。AssetManager 在 Step 7 句柄化：
//   getTexture(name) -> TextureHandle；sf::Texture / SDL_Texture 藏入实现内部。
// 音频（SoundBuffer）不做句柄：调用点少，SDL3 切换时（Step 10）直改 SDL_mixer。
namespace eng {

struct TextureHandle {
    uint32_t id = 0;

    [[nodiscard]] bool isValid() const { return id != 0; }

    bool operator==(const TextureHandle& o) const { return id == o.id; }
    bool operator!=(const TextureHandle& o) const { return id != o.id; }
};

struct FontHandle {
    uint32_t id = 0;

    [[nodiscard]] bool isValid() const { return id != 0; }

    bool operator==(const FontHandle& o) const { return id == o.id; }
    bool operator!=(const FontHandle& o) const { return id != o.id; }
};

} // namespace eng

// unordered_map key 支持
template <>
struct std::hash<eng::TextureHandle> {
    size_t operator()(const eng::TextureHandle& h) const noexcept { return std::hash<uint32_t>()(h.id); }
};

template <>
struct std::hash<eng::FontHandle> {
    size_t operator()(const eng::FontHandle& h) const noexcept { return std::hash<uint32_t>()(h.id); }
};
