//
// Created by MINEC on 2026/1/29.
//

#pragma once
#ifndef SERVER_BUILD
#include <string>
#include <unordered_map>
#include <Animation.h>
#include <fstream>

#include "AssetManager.h"
#include <nlohmann/json_fwd.hpp>
#include "ConfigManager.h"
#include "Logger.h"

using json = nlohmann::json;

class FrameManager {
public:
    static FrameManager& getInstance() {
        static FrameManager instance;
        return instance;
    }

    std::vector<Animation::Frame>* getFrame(const std::string& name) {
        if (!frames.contains(name)) {
            LOG_ERROR_FMT("Error: animation {} does not exist!", name);
            return nullptr;
        }
        return &frames[name];
    }

    void loadFrame() {
        loadFrameFromJson(CONFIG.assets.frames["mario"].c_str());
        loadFrameFromJson(CONFIG.assets.frames["box"].c_str());
        loadFrameFromJson(CONFIG.assets.frames["fireball"].c_str());
        loadFrameFromJson(CONFIG.assets.frames["goomba"].c_str());
    }

    void loadFrameFromJson(const char* path);

private:
    FrameManager() = default;
    std::unordered_map<std::string, std::vector<Animation::Frame>> frames{};
};
#endif