//
// Created by MINEC on 2026/5/8.
//
#ifndef SERVER_BUILD
#include "AssetManager.h"

#include "Logger.h"

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
            // std::cout << "loading: " << entry.path().string() << "..." << std::endl;
            const std::string file_name = entry.path().filename().string().substr(0, entry.path().filename().string().size() - 4);
            if (sf::Texture& texture = textures[file_name]; texture.loadFromFile(entry.path().string())) {
                // std::cout << file_name + " loaded successfully! size: " << texture.getSize().x << " x " << texture.getSize().y << std::endl;
            } else {
                LOG_ERROR_FMT("unable to load {}", entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR(ex.what());
    }
}

sf::Texture& AssetManager::getTexture(const std::string& name) {
    if (!textures.contains(name)) {
        LOG_ERROR_FMT("Texture {} does not exist!", name);
        return textures["default"];
    }
    return textures[name];
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

const sf::Texture& AssetManager::getTexture(const eng::TextureHandle h) {
    if (h.isValid() && h.id <= handle_texture_names.size()) {
        const std::string& name = handle_texture_names[h.id - 1];
        if (textures.contains(name)) {
            return textures[name];
        }
        LOG_ERROR_FMT("Texture {} (handle {}) does not exist!", name, h.id);
    } else {
        LOG_ERROR_FMT("Invalid texture handle: {}", h.id);
    }
    return textures["default"];
}

eng::Vec2u AssetManager::getTextureSize(const eng::TextureHandle h) {
    return getTexture(h).getSize();
}

eng::FontHandle AssetManager::getFontHandle() {
    return eng::FontHandle{1};
}

const sf::Font& AssetManager::getFont(const eng::FontHandle h) {
    (void)h;   // 引擎当前只有一款字体
    return getFont();
}

const sf::Font& AssetManager::getFont() {
    if (!have_load_font) {
        font.loadFromFile(CONFIG.assets.font);
        have_load_font = true;
    }
    return font;
}

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
            // std::cout << "loading: " << entry.path().string() << "..." << std::endl;
            const std::string file_name = entry.path().filename().string().substr(0, entry.path().filename().string().size() - 4);
            if (sf::SoundBuffer& sound_buffer = soundBuffers[file_name]; sound_buffer.loadFromFile(entry.path().string())) {
                // std::cout << file_name + " loaded successfully! size: " << texture.getSize().x << " x " << texture.getSize().y << std::endl;
            } else {
                LOG_ERROR_FMT("unable to load {}", entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR(ex.what());
    }
}

sf::SoundBuffer& AssetManager::getSoundBuffer(const std::string& name) {
    if (!soundBuffers.contains(name)) {
        LOG_ERROR_FMT("SoundBuffer {} does not exist!", name);
        return soundBuffers["default"];
    }
    return soundBuffers[name];
}
#endif