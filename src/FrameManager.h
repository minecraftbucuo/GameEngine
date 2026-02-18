//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <string>
#include <unordered_map>
#include <Animation.h>
#include <iostream>

#include "AssetManager.h"

class FrameManager {
public:
    static FrameManager& getInstance() {
        static FrameManager instance;
        return instance;
    }

    std::vector<Animation::Frame>* getFrame(const std::string& name) {
        if (frames.find(name) == frames.end()) {
            std::cerr << "Error: animation " << name << " does not exist!" << std::endl;
            return nullptr;
        }
        return &frames[name];
    }

    void loadFrame() {
        loadMarioFrame();
    }

    void loadMarioFrame() {
        // right_small_normal
        std::vector<Animation::Frame>& right_small_normal = frames["right_small_normal"];
        std::vector<Animation::Frame>& left_small_normal = frames["left_small_normal"];
        sf::Texture* mario_texture = &AssetManager::getInstance().getTexture("mario_bros");
        right_small_normal.push_back({mario_texture, sf::IntRect(80, 32, 15, 16)});
        right_small_normal.push_back({mario_texture, sf::IntRect(96, 32, 16, 16)});
        right_small_normal.push_back({mario_texture, sf::IntRect(112, 32, 16, 16)});

        for (auto& frame : right_small_normal) {
            frame.scale = {4.f, 4.f};
        }

        left_small_normal = right_small_normal;
        for (auto& frame : left_small_normal) {
            frame.origin = {static_cast<float>(frame.textureRect.width), 0.f};
            frame.scale = {-4.f, 4.f};
        }
    }

private:
    FrameManager() = default;
    std::unordered_map<std::string, std::vector<Animation::Frame>> frames{};
};
