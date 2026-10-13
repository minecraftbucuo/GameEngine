//
// Created by MINEC on 2026/4/4.
//

#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

// 全局配置访问入口。展开后走 ConfigManager::instance()（可替换），
// 而非直接取单例——这样测试替换一处实例即对全部 109 个使用点生效。
// 保留宏是为了不改动既有调用点；后续按需把热点逐步换成构造注入。
#define CONFIG (ConfigManager::instance())

using json = nlohmann::json;

class ConfigManager {
public:
    // 默认配置文件路径（相对 exe 目录，与历史行为一致）
    static constexpr const char* DEFAULT_PATH = "./Asset/config.json";

    // 进程级单例实例（默认生效者）
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // 当前生效实例：默认即 getInstance()；测试可用 setInstance() 换成独立实例。
    // 全部 CONFIG 访问都经此，替换一处即全局生效。
    static ConfigManager& instance() {
        return active_instance ? *active_instance : getInstance();
    }

    // 替换当前生效实例；传 nullptr 恢复默认单例。
    // 供测试在 setup / teardown 成对调用。非线程安全：仅用于启动期或测试，
    // 勿在运行中并发调用（读侧是无锁裸指针，换取热路径零开销）。
    static void setInstance(ConfigManager* cm) {
        active_instance = cm;
    }

    // 允许测试/工具构造独立实例（生产代码请一律用 getInstance() / instance()）
    ConfigManager() = default;

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
    // 被 setInstance() 替换的实例；nullptr = 使用 getInstance() 的默认单例。
    // 裸指针 + 无锁读：CONFIG 在游戏热路径上被高频访问，不做原子操作。
    inline static ConfigManager* active_instance = nullptr;

    // load() / save() 共用的路径（原先两处各自硬编码字面量）
    std::string configPath{DEFAULT_PATH};

    void parseWindow(const json& j);

    void parseAssets(const json& j);

    void parseNetwork(const json& j);

    void parseGame(const json& j);
};