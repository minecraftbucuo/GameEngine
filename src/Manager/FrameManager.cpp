//
// Created by MINEC on 2026/2/19.
//

#include "FrameManager.h"
#include <nlohmann/json.hpp>

void FrameManager::loadMarioFrame() {
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
