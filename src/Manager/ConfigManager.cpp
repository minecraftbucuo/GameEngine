//
// Created by MINEC on 2026/4/4.
//

#include "ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

#include "Logger.h"

bool ConfigManager::load() {
    try {
        const auto path = "./Asset/config.json";
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

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Config parse error: {}", e.what());
        return false;
    }
}

void ConfigManager::parseWindow(const json& j) {
    window.width = j.value("width", window.width);
    window.height = j.value("height", window.height);
    window.title = j.value("title", window.title);
    window.fps = j.value("fps", window.fps);
}

void ConfigManager::parseAssets(const json& j) {
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
    if (j.contains("models")) {
        for (auto& [key, value] : j["models"].items()) {
            assets.models[key] = value.get<std::string>();
        }
    }
    assets.font = j.value("font", assets.font);
}

void ConfigManager::parseNetwork(const json& j) {
    network.serverIp = j.value("serverIp", network.serverIp);
    network.port = j.value("port", network.port);
    network.tickRate = j.value("tickRate", network.tickRate);
    network.timeout = j.value("timeout", network.timeout);
}

void ConfigManager::parseGame(const json& j) {
    game.gravity = j.value("gravity", game.gravity);
    game.playerSpeed = j.value("playerSpeed", game.playerSpeed);
    game.jumpForce = j.value("jumpForce", game.jumpForce);
    game.debug = j.value("debug", game.debug);
    game.defaultBlockSize = j.value("defaultBlockSize", game.defaultBlockSize);
    game.shootDelay = j.value("shootDelay", game.shootDelay);
    game.fireballSpeedY = j.value("fireballSpeedY", game.fireballSpeedY);
    game.fireBallTTL = j.value("fireBallTTL", game.fireBallTTL);
}

bool ConfigManager::save() {
    try {
        const auto path = "./Asset/config.json";

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
        config["network"]["tickRate"] = network.tickRate;
        config["network"]["timeout"] = network.timeout;

        // Game
        config["game"]["gravity"] = game.gravity;
        config["game"]["playerSpeed"] = game.playerSpeed;
        config["game"]["jumpForce"] = game.jumpForce;
        config["game"]["fireballSpeedY"] = game.fireballSpeedY;
        config["game"]["defaultBlockSize"] = game.defaultBlockSize;
        config["game"]["shootDelay"] = game.shootDelay;
        config["game"]["fireBallTTL"] = game.fireBallTTL;
        config["game"]["debug"] = game.debug;

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




