//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <string>
#include <unordered_map>
#include <Animation.h>
#include <iostream>

#include "AssetManager.h"

class AnimationManager {
public:
    static AnimationManager& getInstance() {
        static AnimationManager instance;
        return instance;
    }

    Animation& getAnimation(const std::string& name) {
        if (animations.find(name) == animations.end()) {
            std::cerr << "Error: animation " << name << " does not exist!" << std::endl;
            return animations["default"];
        }
        return animations[name];
    }

    void loadAnimation() {
        loadMarioAnimation();
    }

    void loadMarioAnimation() {
        // right_small_normal
        Animation& right_small_normal = animations["right_small_normal"];
        Animation& left_small_normal = animations["left_small_normal"];
        sf::Texture* mario_texture = &AssetManager::getInstance().getTexture("mario_bros");
        // right_small_normal.addFrame({mario_texture, sf::IntRect(178, 32, 12, 16)});
        right_small_normal.addFrame({mario_texture, sf::IntRect(80, 32, 15, 16)});
        right_small_normal.addFrame({mario_texture, sf::IntRect(96, 32, 16, 16)});
        right_small_normal.addFrame({mario_texture, sf::IntRect(112, 32, 16, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(144, 32, 16, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(130, 32, 14, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(160, 32, 15, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(320, 8, 16, 24)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(241, 33, 16, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(194, 32, 12, 16)});
        // right_small_normal.addFrame({mario_texture, sf::IntRect(210, 33, 12, 16)});

        for (auto& frame : right_small_normal.getFrames()) {
            frame.scale = {5.f, 5.f};
        }

        left_small_normal = right_small_normal;
        for (auto& frame : left_small_normal.getFrames()) {
            frame.origin = {static_cast<float>(frame.textureRect.width), 0.f};
            frame.scale = {-5.f, 5.f};
        }
    }

private:
    AnimationManager() = default;
    std::unordered_map<std::string, Animation> animations{};
};
