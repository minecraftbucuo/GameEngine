//
// Created by MINEC on 2026/4/4.
//

#include "ConfigManager.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <system_error>
#include <type_traits>
#include <nlohmann/json.hpp>

#include "Logger.h"

bool ConfigManager::load() {
    return load(configPath);
}

bool ConfigManager::load(const std::string& path) {
    // 先记住路径：load 与 save 共用同一份 configPath（原先两处各自硬编码字面量）
    configPath = path;
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR_FMT("Failed to open config file: {}", path);
            return false;
        }

        json config;
        file >> config;

        parseWindow(config["window"]);
        parseAssets(config["assets"]);
        parseNetwork(config["network"]);
        parseGame(config["game"]);
        parseDebugPanel(config["debugPanel"]);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Config parse error: {}", e.what());
        return false;
    }
}

void ConfigManager::reset() {
    // 用声明处的默认成员初始化器重建各段，避免把默认值再抄一遍（抄了就会漂移）
    window = WindowConfig{};
    assets = AssetsConfig{};
    network = NetworkConfig{};
    game = GameConfig{};
    debugPanel = DebugPanelConfig{};
}

void ConfigManager::parseWindow(const json& j) {
    // 该段整体缺失时 config["window"] 是 null —— 直接 value() 会抛
    // type_error.306，导致整份配置加载失败。按「缺段即保留当前值」处理，
    // 与下方 value(key, 默认值) 的容错语义保持一致。
    if (!j.is_object()) return;

    window.width = j.value("width", window.width);
    window.height = j.value("height", window.height);
    window.title = j.value("title", window.title);
    window.fps = j.value("fps", window.fps);
}

void ConfigManager::parseAssets(const json& j) {
    if (!j.is_object()) return;

    if (j.contains("textures")) {
        for (auto& [key, value] : j["textures"].items()) {
            assets.textures[key] = value.get<std::string>();
        }
    }
    if (j.contains("sounds")) {
        for (auto& [key, value] : j["sounds"].items()) {
            assets.sounds[key] = value.get<std::string>();
        }
    }
    if (j.contains("animations")) {
        for (auto& [key, value] : j["animations"].items()) {
            assets.frames[key] = value.get<std::string>();
        }
    }
    assets.font = j.value("font", assets.font);
}

void ConfigManager::parseNetwork(const json& j) {
    if (!j.is_object()) return;

    network.serverIp = j.value("serverIp", network.serverIp);
    network.port = j.value("port", network.port);
    network.webBridgePort = j.value("webBridgePort", network.webBridgePort);
    network.tickRate = j.value("tickRate", network.tickRate);
    network.timeout = j.value("timeout", network.timeout);
}

void ConfigManager::parseGame(const json& j) {
    if (!j.is_object()) return;

    game.levelMap = j.value("levelMap", game.levelMap);
    game.gravity = j.value("gravity", game.gravity);
    game.playerSpeed = j.value("playerSpeed", game.playerSpeed);
    game.jumpForce = j.value("jumpForce", game.jumpForce);
    game.debug = j.value("debug", game.debug);
    game.defaultBlockSize = j.value("defaultBlockSize", game.defaultBlockSize);
    game.shootDelay = j.value("shootDelay", game.shootDelay);
    game.fireballSpeedY = j.value("fireballSpeedY", game.fireballSpeedY);
    game.fireBallTTL = j.value("fireBallTTL", game.fireBallTTL);
    game.physicsVelocityIterations = j.value("physicsVelocityIterations", game.physicsVelocityIterations);
    game.physicsPositionIterations = j.value("physicsPositionIterations", game.physicsPositionIterations);
    game.physicsFixedStep = j.value("physicsFixedStep", game.physicsFixedStep);
}

bool ConfigManager::save() {
    try {
        const auto& path = configPath;

        // 先读取现有配置，保留 assets 等未修改的部分
        json config;
        {
            std::ifstream in(path);
            if (in.is_open()) {
                in >> config;
            }
        }

        // Window
        config["window"]["width"] = window.width;
        config["window"]["height"] = window.height;
        config["window"]["title"] = window.title;
        config["window"]["fps"] = window.fps;

        // Network
        config["network"]["serverIp"] = network.serverIp;
        config["network"]["port"] = network.port;
        config["network"]["webBridgePort"] = network.webBridgePort;
        config["network"]["tickRate"] = network.tickRate;
        config["network"]["timeout"] = network.timeout;

        // Game
        config["game"]["levelMap"] = game.levelMap;
        config["game"]["gravity"] = game.gravity;
        config["game"]["playerSpeed"] = game.playerSpeed;
        config["game"]["jumpForce"] = game.jumpForce;
        config["game"]["fireballSpeedY"] = game.fireballSpeedY;
        config["game"]["defaultBlockSize"] = game.defaultBlockSize;
        config["game"]["shootDelay"] = game.shootDelay;
        config["game"]["fireBallTTL"] = game.fireBallTTL;
        config["game"]["debug"] = game.debug;
        config["game"]["physicsVelocityIterations"] = game.physicsVelocityIterations;
        config["game"]["physicsPositionIterations"] = game.physicsPositionIterations;
        config["game"]["physicsFixedStep"] = game.physicsFixedStep;

        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_ERROR_FMT("Failed to open config file for writing: {}", path);
            return false;
        }
        file << config.dump(4);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Config save error: {}", e.what());
        return false;
    }
}

namespace {

// 逐字段读取：类型不符时保留原值，而不是抛到 load() 的 catch 里。
// 其余各段用的是裸 value()，写错一个字段就会让整份配置加载失败、全部回落默认值。
// 调试面板是纯可选功能，配错不应该连累游戏启动，所以这里单独做容错。
template <typename T>
void readField(const json& j, const char* key, T& out) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return;

    try {
        if constexpr (std::is_integral_v<T>) {
            // 手写配置里 8090.0 这种写法很常见，按整数截断而不是判定为类型错误
            if (it->is_number_float()) {
                out = static_cast<T>(it->get<double>());
                return;
            }
        }
        out = it->get<T>();
    } catch (const json::exception&) {
        // 类型不符的字段视同未提供，保留默认值
    }
}

} // namespace

void ConfigManager::parseDebugPanel(const json& j) {
    // 整段缺失时 config["debugPanel"] 是 null，保持默认值即可
    if (!j.is_object()) return;

    readField(j, "enabled", debugPanel.enabled);
    readField(j, "host", debugPanel.host);
    readField(j, "port", debugPanel.port);
    readField(j, "token", debugPanel.token);
    readField(j, "snapshotHz", debugPanel.snapshotHz);
    readField(j, "maxObjects", debugPanel.maxObjects);
    readField(j, "idleSkipSeconds", debugPanel.idleSkipSeconds);
    readField(j, "webRoot", debugPanel.webRoot);

    // 越界值会让 bind 失败或快照开销失控，统一夹到安全区间
    debugPanel.port = std::clamp(debugPanel.port, 1, 65535);
    debugPanel.snapshotHz = std::clamp(debugPanel.snapshotHz, 1, 60);
    debugPanel.maxObjects = std::clamp(debugPanel.maxObjects, 1, 100000);
    debugPanel.idleSkipSeconds = std::clamp(debugPanel.idleSkipSeconds, 0, 3600);
}

namespace {

// 未设置与设为空串统一按"没给"处理，简化各调用点判断
std::string readEnv(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

bool parseEnvBool(const std::string& text, bool* out) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char c : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes") {
        *out = true;
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no") {
        *out = false;
        return true;
    }
    return false;
}

// 整段解析：多余字符（"8090abc"）算非法，避免 "8090.5" 之类被静默截断
bool parseEnvInt(const std::string& text, int* out) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    int value = 0;
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) return false;
    *out = value;
    return true;
}

} // namespace

void applyDebugPanelEnvOverrides(ConfigManager::DebugPanelConfig& cfg) {
    if (const std::string v = readEnv("MARIO_PANEL_ENABLE"); !v.empty()) {
        bool enabled = false;
        if (parseEnvBool(v, &enabled)) {
            cfg.enabled = enabled;
        } else {
            LOG_WARN_FMT("MARIO_PANEL_ENABLE='{}' is not a boolean, ignored", v);
        }
    }

    if (const std::string v = readEnv("MARIO_PANEL_HOST"); !v.empty()) {
        cfg.host = v;
    }

    if (const std::string v = readEnv("MARIO_PANEL_PORT"); !v.empty()) {
        int port = 0;
        if (parseEnvInt(v, &port)) {
            cfg.port = std::clamp(port, 1, 65535);
        } else {
            LOG_WARN_FMT("MARIO_PANEL_PORT='{}' is not a valid port, ignored", v);
        }
    }

    if (const std::string v = readEnv("MARIO_PANEL_TOKEN"); !v.empty()) {
        cfg.token = v;
    }

    if (const std::string v = readEnv("MARIO_PANEL_HZ"); !v.empty()) {
        int hz = 0;
        if (parseEnvInt(v, &hz)) {
            cfg.snapshotHz = std::clamp(hz, 1, 60);
        } else {
            LOG_WARN_FMT("MARIO_PANEL_HZ='{}' is not an integer, ignored", v);
        }
    }

    if (const std::string v = readEnv("MARIO_PANEL_IDLE_SKIP"); !v.empty()) {
        int seconds = 0;
        if (parseEnvInt(v, &seconds)) {
            cfg.idleSkipSeconds = std::clamp(seconds, 0, 3600);
        } else {
            LOG_WARN_FMT("MARIO_PANEL_IDLE_SKIP='{}' is not an integer, ignored", v);
        }
    }

    if (const std::string v = readEnv("MARIO_PANEL_WEBROOT"); !v.empty()) {
        cfg.webRoot = v;
    }
}




