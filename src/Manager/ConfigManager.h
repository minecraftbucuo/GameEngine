//
// Created by MINEC on 2026/4/4.
//

#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

#define CONFIG ConfigManager::getInstance()

using json = nlohmann::json;

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool load();

    bool save();

    // Window 配置
    struct WindowConfig {
        unsigned width = 1200;
        unsigned height = 960;
        std::string title = "GameEngine";
        int fps = 165;
    } window{};

    // Assets 配置
    struct AssetsConfig {
        std::unordered_map<std::string, std::string> textures;
        std::unordered_map<std::string, std::string> sounds;
        std::unordered_map<std::string, std::string> frames;
        std::unordered_map<std::string, std::string> models;
        std::string font = "./Asset/Font/Minecraft_AE.ttf";
    } assets;

    // Network 配置
    struct NetworkConfig {
        std::string serverIp = "127.0.0.1";
        int port = 6666;
        int webBridgePort = 8081;   // WEB 联机：websockify 桥端口（port 键仍归桌面 TCP 直连）
        int tickRate = 128;
        float timeout = 5.0f;
    } network{};

    // Game 配置
    struct GameConfig {
        float gravity = 3200.0f;
        float playerSpeed = 500.0f;
        float jumpForce = 900.0f;
        float fireballSpeedY = -400.0f;
        float defaultBlockSize = 64.f;
        int shootDelay = 300;
        int fireBallTTL = 10000;
        bool debug = true;
        // Box2D 物理参数
        int physicsVelocityIterations = 8;
        int physicsPositionIterations = 3;
        float physicsFixedStep = 1.0f / 60.0f;
    } game{};

    // 便捷访问方法
    std::string getTexturePath(const std::string& name) const {
        const auto it = assets.textures.find(name);
        return it != assets.textures.end() ? it->second : "";
    }

    std::string getSoundPath(const std::string& name) const {
        const auto it = assets.sounds.find(name);
        return it != assets.sounds.end() ? it->second : "";
    }

    std::string getModelPath(const std::string& name) const {
        const auto it = assets.models.find(name);
        return it != assets.models.end() ? it->second : "";
    }

private:
    ConfigManager() = default;

    void parseWindow(const json& j);

    void parseAssets(const json& j);

    void parseNetwork(const json& j);

    void parseGame(const json& j);
};