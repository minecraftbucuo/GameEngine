//
// Created by MINEC on 2026/9/18.
//
// 超级马里奥关卡地图加载器：把原本硬编码在场景类里的地图数据外置到
// Asset/maps/ 下的关卡 JSON（具体文件由 config.json 的 game.levelMap 指定），
// 单机/联机场景共用静态部分。

#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ConfigManager.h"

class Scene; // 全局命名空间前置声明

namespace MapLoader {

// 动态出生点数据（仅纯数据：敌人等玩法对象的创建仍归各场景，加载器不依赖玩法类）
struct EnemySpawn {
    float x = 0.f;
    float y = 0.f;
    float speed = 0.f;
};

struct MapDynamicData {
    float player_spawn_x = 100.f;
    float player_spawn_y = 100.f;
    std::vector<EnemySpawn> goombas;
    bool has_bowser = false;
    EnemySpawn bowser;
};

// 静态地形（左墙/砖块/箱子/地面）加载进场景：
// 经虚函数 Scene::addObject 注入，两个场景各自的 override 会完成碰撞注册
bool loadStaticObjects(Scene& scene, const std::string& json_path = CONFIG.game.levelMap);

// 动态出生点数据读取（单机场景用它生成马里奥/板栗仔/乌龟大王；联机只用玩家出生点）
std::optional<MapDynamicData> loadDynamicData(const std::string& json_path = CONFIG.game.levelMap);

} // namespace MapLoader
