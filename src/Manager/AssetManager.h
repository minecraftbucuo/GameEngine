//
// Created by MINEC on 2026/1/29.
//

#pragma once
#ifndef SERVER_BUILD

#ifdef ENGINE_SDL3
// ── SDL3 分支（SDL3 迁移 Step 9；Step 10 生效）─────────────────────────
// 资源本体 SDL 化，游戏层句柄 API 签名不变；SFML 分支（#else）为脚手架期原样。
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "ConfigManager.h"
#include "Core/Types.h"
#include "Render/Handles.h"

typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Texture SDL_Texture;
typedef struct MIX_Audio  MIX_Audio;
typedef struct MIX_Mixer  MIX_Mixer;

class AssetManager {
public:
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    void loadTexture(const char* path);

    // 名字 → 句柄（首次访问时分配 id；游戏层从此只持有句柄）
    eng::TextureHandle getTextureHandle(const std::string& name);
    // 句柄 → SDL 纹理（懒创建：surface 首次被取用时经当前渲染器上传 GPU；
    // 渲染器未创建或句柄无效返回 nullptr，调用方判空跳过）
    SDL_Texture* getTexture(eng::TextureHandle h);
    // 句柄 → 纹理尺寸（像素；布局计算用，如背景铺满缩放）
    eng::Vec2u getTextureSize(eng::TextureHandle h);

    // 字体：SDL_ttf 字体对象与字号绑定，由 RendererSDL3 按字号内部缓存打开，
    // 此处只提供字体文件路径（引擎当前只有一款字体，固定 id=1）
    eng::FontHandle getFontHandle();
    const std::string& getFontPath(eng::FontHandle h);

    // 音频（SDL_mixer 3.2 track API；加载侧在此，播放侧 Step 10 接入）
    void loadSoundBuffer(const char* path);
    void addSoundBuffer(const std::string& name, MIX_Audio* audio) {
        soundBuffers[name] = audio;
    }
    // 未找到时返回 nullptr 并记日志（SFML 版返回 default 占位，SDL3 版调用方判空）
    MIX_Audio* getSoundBuffer(const std::string& name);
    // 混音器（懒初始化；MIX_CreateTrack/播放侧用）
    MIX_Mixer* getMixer();

private:
    AssetManager() = default;
    struct TexEntry {
        SDL_Surface* surface = nullptr;   // 贴图原始像素（渲染器建立前的形态）
        SDL_Texture* texture = nullptr;   // 上传 GPU 后的形态（此后 surface 已销毁）
    };
    std::unordered_map<std::string, TexEntry> textures{};
    std::unordered_map<std::string, MIX_Audio*> soundBuffers{};
    MIX_Mixer* mixer = nullptr;
    bool mix_inited = false;
    std::string font_path{};
    bool have_font_path = false;

    // 句柄内部表：name → id 与 id → name 双向（id 从 1 递增，0 为无效）
    std::unordered_map<std::string, uint32_t> texture_handle_ids{};
    std::vector<std::string> handle_texture_names{};   // 下标 = id - 1

    MIX_Mixer* ensureMixer();
};
#else
// ── SFML 分支（脚手架期实现，迁移 Step 11 删除）────────────────────────
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
#endif
