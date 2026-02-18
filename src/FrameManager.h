//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <string>
#include <unordered_map>
#include <Animation.h>
#include <fstream>
#include <iostream>

#include "AssetManager.h"
#include "json.hpp"
using json = nlohmann::json;

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
        std::ifstream file("./Asset/SuperMario/source/data/player/mario.json");

        json data = json::parse(file);
        for (const auto& frames_data : data["frames"]) {
            std::vector<Animation::Frame>& frame = frames[frames_data["frame_name"]];
            sf::Texture* frame_texture = &AssetManager::getInstance().getTexture(frames_data["texture_name"]);
            for (const auto& frame_data : frames_data["frame"]) {
                frame.push_back({
                    frame_texture,
                    sf::IntRect(
                        frame_data["textureRect"]["rectLeft"],
                        frame_data["textureRect"]["rectTop"],
                        frame_data["textureRect"]["rectWidth"],
                        frame_data["textureRect"]["rectHeight"]
                    ),
                    {frame_data["origin"]["x"], frame_data["origin"]["y"]},
                    {frame_data["scale"]["x"], frame_data["scale"]["y"]},
                    frame_data["duration"]
                });
            }
        }

        file.close();
    }

private:
    FrameManager() = default;
    std::unordered_map<std::string, std::vector<Animation::Frame>> frames{};
};
