//
// Created by MINEC on 2026/1/29.
//

#pragma once
#ifndef SERVER_BUILD
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "ConfigManager.h"
#include "Core/Types.h"
#include "Render/Handles.h"

class AssetManager {
public:
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    void loadTexture(const char* path);

    sf::Texture& getTexture(const std::string& name);

    // ── 句柄 API（SDL3 迁移 Step 5 新增；游戏层迁移到句柄后旧按名 API 移除）──
    // 名字 → 句柄（首次访问时分配 id；游戏层从此只持有句柄）
    eng::TextureHandle getTextureHandle(const std::string& name);
    // 句柄 → 纹理（Renderer 实现内部使用；句柄无效返回占位纹理）
    const sf::Texture& getTexture(eng::TextureHandle h);
    // 句柄 → 纹理尺寸（像素；布局计算用，如背景铺满缩放）
    eng::Vec2u getTextureSize(eng::TextureHandle h);
    // 字体句柄（当前引擎只有一款字体，固定 id=1）
    eng::FontHandle getFontHandle();
    const sf::Font& getFont(eng::FontHandle h);

    void addTexture(const std::string& name, const sf::Texture& texture) {
        textures[name] = texture;
    }

    const sf::Font& getFont();

    void loadSoundBuffer(const char* path);

    void addSoundBuffer(const std::string& name, const sf::SoundBuffer& sound_buffer) {
        soundBuffers[name] = sound_buffer;
    }

    sf::SoundBuffer& getSoundBuffer(const std::string& name);

private:
    AssetManager() = default;
    std::unordered_map<std::string, sf::Texture> textures{};
    std::unordered_map<std::string, sf::SoundBuffer> soundBuffers{};
    sf::Font font{};
    bool have_load_font = false;

    // 句柄内部表：name → id 与 id → name 双向（id 从 1 递增，0 为无效）
    std::unordered_map<std::string, uint32_t> texture_handle_ids{};
    std::vector<std::string> handle_texture_names{};   // 下标 = id - 1
};
#endif