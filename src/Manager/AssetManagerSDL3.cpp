//
// Created by MINEC on 2026/8/23.
//
#ifndef SERVER_BUILD

// 【SDL3 终态实现 — 迁移 Step 9 纯新增；Step 10 生效（替换 AssetManager.cpp 的 SFML 版）】
// AssetManager 的 SDL3 内部实现：贴图走 SDL_image（surface 懒上传纹理），
// 音频走 SDL_mixer 3.2 track API（MIX_LoadAudio 全量预解码，对齐 sf::SoundBuffer 语义）。
// 只在定义了 ENGINE_SDL3 时参与编译（挂在 EngineSDL3 编译验证目标上）。
#if defined(ENGINE_SDL3)

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>   // SDL3_image 3.2 已无 IMG_Init：解码器自动注册
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>

#include "Manager/AssetManager.h"
#include "Render/RendererSDL3Internal.h"
#include "Logger.h"

// ── 音频：混音器懒初始化 ──

MIX_Mixer* AssetManager::ensureMixer() {
    if (mixer) return mixer;
    if (!mix_inited) {
        if (!MIX_Init()) {   // SDL_mixer 3.2 无 flags 参数，按编译进来的解码器注册
            LOG_ERROR_FMT("MIX_Init failed: {}", SDL_GetError());
            return nullptr;
        }
        mix_inited = true;
    }
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer) LOG_ERROR_FMT("MIX_CreateMixerDevice failed: {}", SDL_GetError());
    return mixer;
}

MIX_Mixer* AssetManager::getMixer() {
    return ensureMixer();
}

// ── 贴图 ──

void AssetManager::loadTexture(const char* path) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR_FMT("Path does not exist : {}", path);
        return;
    }
    if (!std::filesystem::is_directory(path)) {
        LOG_ERROR_FMT("The provided path is not a directory : {}", path);
        return;
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            std::string extension = entry.path().extension().string();
            std::ranges::transform(extension, extension.begin(), ::tolower);
            if (extension != ".png") continue;
            const std::string file_name = entry.path().filename().string().substr(
                0, entry.path().filename().string().size() - 4);
            // 此阶段只解码到 surface；等渲染器建立后首次取用时再上传 GPU
            if (SDL_Surface* surface = IMG_Load(entry.path().string().c_str())) {
                textures[file_name] = TexEntry{surface, nullptr};
            } else {
                LOG_ERROR_FMT("unable to load {}", entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR(ex.what());
    }
}

eng::TextureHandle AssetManager::getTextureHandle(const std::string& name) {
    if (const auto it = texture_handle_ids.find(name); it != texture_handle_ids.end()) {
        return eng::TextureHandle{it->second};
    }
    const auto id = static_cast<uint32_t>(handle_texture_names.size()) + 1;
    handle_texture_names.push_back(name);
    texture_handle_ids[name] = id;
    return eng::TextureHandle{id};
}

SDL_Texture* AssetManager::getTexture(const eng::TextureHandle h) {
    if (!h.isValid() || h.id > handle_texture_names.size()) {
        LOG_ERROR_FMT("Invalid texture handle: {}", h.id);
        return nullptr;
    }
    const std::string& name = handle_texture_names[h.id - 1];
    if (!textures.contains(name)) {
        LOG_ERROR_FMT("Texture {} (handle {}) does not exist!", name, h.id);
        return nullptr;
    }
    TexEntry& entry = textures[name];
    // 懒上传：surface → GPU 纹理（需渲染器已建立；上传成功后释放 surface）
    if (!entry.texture && entry.surface) {
        SDL_Renderer* renderer = eng::detail::sdl3Renderer();
        if (!renderer) return nullptr;   // 渲染器未建立（正常流程 createWindow 先于绘制）
        entry.texture = SDL_CreateTextureFromSurface(renderer, entry.surface);
        if (entry.texture) {
            SDL_DestroySurface(entry.surface);
            entry.surface = nullptr;
        } else {
            LOG_ERROR_FMT("SDL_CreateTextureFromSurface({}) failed: {}", name, SDL_GetError());
            return nullptr;
        }
    }
    return entry.texture;
}

eng::Vec2u AssetManager::getTextureSize(const eng::TextureHandle h) {
    if (!h.isValid() || h.id > handle_texture_names.size()) return eng::Vec2u(0, 0);
    const auto it = textures.find(handle_texture_names[h.id - 1]);
    if (it == textures.end()) return eng::Vec2u(0, 0);
    const TexEntry& entry = it->second;
    if (entry.texture) {
        float w = 0.f, hh = 0.f;
        SDL_GetTextureSize(entry.texture, &w, &hh);
        return eng::Vec2u(static_cast<unsigned>(w), static_cast<unsigned>(hh));
    }
    if (entry.surface) {
        return eng::Vec2u(static_cast<unsigned>(entry.surface->w),
                          static_cast<unsigned>(entry.surface->h));
    }
    return eng::Vec2u(0, 0);
}

// ── 字体 ──

eng::FontHandle AssetManager::getFontHandle() {
    return eng::FontHandle{1};
}

const std::string& AssetManager::getFontPath(const eng::FontHandle h) {
    (void)h;   // 引擎当前只有一款字体
    if (!have_font_path) {
        font_path = CONFIG.assets.font;
        have_font_path = true;
    }
    return font_path;
}

// ── 音频 ──

void AssetManager::loadSoundBuffer(const char* path) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR_FMT("Path does not exist : {}", path);
        return;
    }
    if (!std::filesystem::is_directory(path)) {
        LOG_ERROR_FMT("The provided path is not a directory : {}", path);
        return;
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            std::string extension = entry.path().extension().string();
            std::ranges::transform(extension, extension.begin(), ::tolower);
            if (extension != ".ogg") continue;
            const std::string file_name = entry.path().filename().string().substr(
                0, entry.path().filename().string().size() - 4);
            MIX_Mixer* m = ensureMixer();
            if (!m) {
                LOG_ERROR_FMT("unable to load {} (no mixer)", entry.path().string());
                continue;
            }
            // predecode=true：全量解码进内存，对齐 sf::SoundBuffer 语义（短音效反复播放）
            if (MIX_Audio* audio = MIX_LoadAudio(m, entry.path().string().c_str(), true)) {
                soundBuffers[file_name] = audio;
            } else {
                LOG_ERROR_FMT("unable to load {}", entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR(ex.what());
    }
}

MIX_Audio* AssetManager::getSoundBuffer(const std::string& name) {
    if (!soundBuffers.contains(name)) {
        LOG_ERROR_FMT("SoundBuffer {} does not exist!", name);
        return nullptr;
    }
    return soundBuffers[name];
}

#endif // ENGINE_SDL3
#endif // SERVER_BUILD
