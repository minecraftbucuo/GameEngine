//
// Created by MINEC on 2026/1/29.
//

#pragma once
#ifndef SERVER_BUILD

// 资源管理器（SDL3 终态 — 迁移 Step 11 起单后端）
// 贴图 SDL_image（surface 懒上传 GPU 纹理）、字体 SDL_ttf（路径供按字号打开）、
// 音频 SDL_mixer track API（预解码）。游戏层只持有句柄，资源本体不可见。
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

    // 音频（SDL_mixer 3.2 track API）
    void loadSoundBuffer(const char* path);
    void addSoundBuffer(const std::string& name, MIX_Audio* audio) {
        soundBuffers[name] = audio;
    }
    // 未找到时返回 nullptr 并记日志（调用方判空）
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
#endif
