//
// Created by MINEC on 2026/9/18.
//

#include "MapLoader.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "Logger.h"
#include "Scene.h"
#include "Ground.h"
#include "Box.h"
#include "Brick.h"

namespace MapLoader {

namespace {

nlohmann::json readJsonFile(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        LOG_ERROR_FMT("Failed to open map file: {}", json_path);
        return {};
    }
    nlohmann::json root;
    file >> root;
    return root;
}

} // namespace

bool loadStaticObjects(Scene& scene, const std::string& json_path) {
    nlohmann::json root;
    try {
        root = readJsonFile(json_path);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Map parse error: {}", e.what());
        return false;
    }
    if (root.is_null() || !root.contains("static")) {
        return false;
    }

    const auto& stat = root["static"];

    // 左墙
    if (stat.contains("wall")) {
        const auto& wall = stat["wall"];
        scene.addObject(std::make_shared<Ground>(
            wall.value("x", 0.f), wall.value("y", 0.f),
            wall.value("width", 10.f), wall.value("height", 960.f),
            wall.value("tag", std::string("ground"))));
    }

    for (const auto& brick : stat.value("bricks", nlohmann::json::array())) {
        scene.addObject(std::make_shared<Brick>(brick.value("x", 0.f), brick.value("y", 0.f)));
    }

    for (const auto& box : stat.value("boxes", nlohmann::json::array())) {
        scene.addObject(std::make_shared<Box>(box.value("x", 0.f), box.value("y", 0.f)));
    }

    // 地面/台阶：存对角两点 (x1,y1)-(x2,y2)，宽高 = 右下角减左上角（与原硬编码一致）
    for (const auto& g : stat.value("grounds", nlohmann::json::array())) {
        const float x1 = g.value("x1", 0.f);
        const float y1 = g.value("y1", 0.f);
        scene.addObject(std::make_shared<Ground>(x1, y1,
                                                 g.value("x2", 0.f) - x1,
                                                 g.value("y2", 0.f) - y1));
    }

    LOG_DEBUG_FMT("Static map loaded from {}", json_path);
    return true;
}

std::optional<MapDynamicData> loadDynamicData(const std::string& json_path) {
    nlohmann::json root;
    try {
        root = readJsonFile(json_path);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Map parse error: {}", e.what());
        return std::nullopt;
    }
    if (root.is_null() || !root.contains("dynamic")) {
        return std::nullopt;
    }

    MapDynamicData data;
    const auto& dyn = root["dynamic"];

    if (dyn.contains("player_spawn")) {
        data.player_spawn_x = dyn["player_spawn"].value("x", data.player_spawn_x);
        data.player_spawn_y = dyn["player_spawn"].value("y", data.player_spawn_y);
    }

    for (const auto& g : dyn.value("goombas", nlohmann::json::array())) {
        EnemySpawn spawn;
        spawn.x = g.value("x", 0.f);
        spawn.y = g.value("y", 793.f);
        spawn.speed = g.value("speed", -100.f);
        data.goombas.push_back(spawn);
    }

    if (dyn.contains("bowser")) {
        data.has_bowser = true;
        data.bowser.x = dyn["bowser"].value("x", 0.f);
        data.bowser.y = dyn["bowser"].value("y", 729.f);
        data.bowser.speed = dyn["bowser"].value("speed", -120.f);
    }

    return data;
}

} // namespace MapLoader
