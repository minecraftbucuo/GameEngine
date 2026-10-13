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
    // 默认配置文件路径（相对 exe 目录，与历史行为一致）
    static constexpr const char* DEFAULT_PATH = "./Asset/config.json";

    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // 用当前 configPath 加载 / 保存
    bool load();

    bool save();

    // 指定路径加载，并记住该路径（后续 save() 也写回这里）。
    // 目的：测试可指向临时 fixture，无需改动进程工作目录、
    // 也无需把测试文件放到固定的 ./Asset/config.json 位置。
    bool load(const std::string& path);

    // 恢复全部字段为默认值（不触碰文件）。
    // 目的：单例状态会跨用例残留，用例前后各 reset() 一次即可干净隔离。
    void reset();

    // 当前生效的配置文件路径
    [[nodiscard]] const std::string& getPath() const {
        return configPath;
    }

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
        std::string font = "./Asset/fonts/Minecraft_AE.ttf";
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
        std::string levelMap = "./Asset/maps/level_1.json";
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

private:
    ConfigManager() = default;

    // load() / save() 共用的路径（原先两处各自硬编码字面量）
    std::string configPath{DEFAULT_PATH};

    void parseWindow(const json& j);

    void parseAssets(const json& j);

    void parseNetwork(const json& j);

    void parseGame(const json& j);
};