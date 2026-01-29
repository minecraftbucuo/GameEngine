//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <SFML/Graphics.hpp>

class AssetManager {
public:
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    void loadTexture(const char* path) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: Path does not exist!" << std::endl;
            return;
        }
        if (!std::filesystem::is_directory(path)) {
            std::cerr << "Error: The provided path is not a directory!" << std::endl;
            return;
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (extension != ".png") continue;
                // std::cout << "loading: " << entry.path().string() << "..." << std::endl;
                const std::string file_name = entry.path().filename().string().substr(0, entry.path().filename().string().size() - 4);
                if (sf::Texture& texture = textures[file_name]; texture.loadFromFile(entry.path().string())) {
                    // std::cout << file_name + " loaded successfully! size: " << texture.getSize().x << " x " << texture.getSize().y << std::endl;
                } else {
                    std::cerr << "Error: unable to load " << entry.path().string() << "!" << std::endl;
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }

    sf::Texture& getTexture(const std::string& name) {
        if (textures.find(name) == textures.end()) {
            std::cerr << "Error: texture " << name << " does not exist!" << std::endl;
            return textures["default"];
        }
        return textures[name];
    }

    void addTexture(const std::string& name, const sf::Texture& texture) {
        textures[name] = texture;
    }

private:
    AssetManager() = default;
    std::unordered_map<std::string, sf::Texture> textures{};
};