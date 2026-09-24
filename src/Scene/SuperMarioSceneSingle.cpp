//
// Created by MINEC on 2026/9/16.
//
// 马里奥单机场景实现：无 NetworkManager。
// 行为与原 SuperMarioScene 的 Local（网页单机）路径完全一致——
// 进场景即生成玩家与敌人，死亡后 R 直接本地重生。

#include "SuperMarioSceneSingle.h"
#ifndef SERVER_BUILD
#include "AssetManager.h"
#include "EventBus.h"
#include "Mario.h"
#include "Ground.h"
#include "Box.h"
#include "Brick.h"
#include "SceneManager.h"
#include "Goomba.h"
#include "Koopa.h"
#include "Bowser.h"
#include "Collision.h"
#include "MapLoader.h"
#include "Render/Renderer.h"

void SuperMarioSceneSingle::init() {
    Scene::init();
    if (is_init) {
        // 重进场景：清上一局会话（对象/碰撞/提示标志）
        resetSession();
        return;
    }
    is_init = true;
    collisionSystem = std::make_unique<CollisionSystem>();
    // SDL3 迁移 6c：背景数据化——按世界视口高度等比缩放铺满（固定视口，
    // 与窗口解耦：重进场景/窗口缩放后背景与 y=857 物理地面始终对齐）
    bg_texture = AssetManager::getInstance().getTextureHandle("level_1");
    const eng::Vec2u tex_size = AssetManager::getInstance().getTextureSize(bg_texture);
    const eng::Vec2u win_size = getWindowSize();
    const float bg_scale = static_cast<float>(win_size.y) / static_cast<float>(tex_size.y);
    bg_dst = eng::FloatRect(0.f, 0.f,
                            static_cast<float>(tex_size.x) * bg_scale,
                            static_cast<float>(tex_size.y) * bg_scale);

    EventBus::getInstance().subscribe<const bool>(
        "PlayerDied",
        [this](const bool flag) -> void {
            this->show_death_screen = flag;
        }
    );
    initStaticObjects();
    // 本地权威：进场景即生成玩家与敌人（原 WEB Local 路径 startServer 后的生成时机）
    initDynamicObjects();
}

void SuperMarioSceneSingle::exit() {
    Scene::exit();
}

void SuperMarioSceneSingle::initStaticObjects() {
    // 地图外置：静态地形（左墙/砖块/箱子/地面）从配置指定的关卡地图加载
    MapLoader::loadStaticObjects(*this);
}

void SuperMarioSceneSingle::initDynamicObjects() {
    if (is_initDynamicObjects) return;
    is_initDynamicObjects = true;

    // 出生点与敌人来自配置指定的关卡地图（位置/速度与原硬编码一致）
    const std::optional<MapLoader::MapDynamicData> map_data = MapLoader::loadDynamicData();
    if (!map_data) {
        LOG_ERROR("Failed to load dynamic map data");
        return;
    }
    std::shared_ptr<Mario> mario = std::make_shared<Mario>(map_data->player_spawn_x, map_data->player_spawn_y);
    this->addObjectWithMap(mario);
    LOG_DEBUG("Create mario");

    for (const auto& spawn : map_data->goombas) {
        this->addObject(std::make_shared<Goomba>(spawn.x, spawn.y, spawn.speed));
    }

    for (const auto& spawn : map_data->koopas) {
        this->addObject(std::make_shared<Koopa>(spawn.x, spawn.y, spawn.speed));
    }

    // 乌龟大王 BOSS：放在前段平地方便测试（正式位置为最终楼梯后 x≈13500）
    if (map_data->has_bowser) {
        this->addObject(std::make_shared<Bowser>(map_data->bowser.x, map_data->bowser.y, map_data->bowser.speed));
    }
}

void SuperMarioSceneSingle::render(eng::Renderer& _renderer) {
    if (bg_texture.isValid()) {
        const eng::Vec2u tex = AssetManager::getInstance().getTextureSize(bg_texture);
        _renderer.drawTexture(bg_texture,
                              eng::FloatRect(0.f, 0.f, static_cast<float>(tex.x), static_cast<float>(tex.y)),
                              bg_dst);
    }
    // SDL3 迁移 6c：对象循环走新虚链（原 Scene::render(_renderer) 会虚转发回旧签名，
    // 使 Mario 等无旧 override 的对象收不到新签名组件渲染）
    renderObjects(_renderer);
    if (show_death_screen) {
        showDeathScreen(_renderer);
    }
}

void SuperMarioSceneSingle::update(eng::Time deltaTime) {
    Scene::update(deltaTime);
    if (this->collisionSystem) {
        this->collisionSystem->checkCollisions();
    }
}

void SuperMarioSceneSingle::addObject(const std::shared_ptr<GameObject>& obj) {
    Scene::addObject(obj);
    if (this->collisionSystem && obj->getComponent<Collision>()) {
        this->collisionSystem->addObject(obj);
    }
}

void SuperMarioSceneSingle::handleEvent(const eng::EngineEvent& event) {
    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        const auto& obj = game_objects[i];
        obj->handleEvent(event);
    }

    // 与基类 Scene::handleEvent 保持一致：窗口变化时同步相机
    // （固定视口下为兜底重设；不调基类版本以免事件被二次转发给对象）
    if (camera && event.type == eng::EventType::WindowResize) {
        camera->resize();
    }

    if (event.type == eng::EventType::MouseButtonPress) {
        const eng::Vec2i pos = getMousePosition();
        LOG_DEBUG_FMT("Mouse clicked at ({}, {})", pos.x, pos.y);
    } else if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::Escape) {
            getSceneManager()->loadScene("MenuScene");
            clearObjects();
        } else if (event.key == eng::Key::R && show_death_screen) {
            show_death_screen = false;
            // 本地权威：死亡后 R 直接重生（与原 Server/Local 分支等价，网络同步为空操作）
            // 重生点同样取自关卡地图的 player_spawn
            if (const auto map_data = MapLoader::loadDynamicData()) {
                std::shared_ptr<Mario> mario = std::make_shared<Mario>(map_data->player_spawn_x, map_data->player_spawn_y);
                this->addObjectWithMap(mario);
                LOG_DEBUG("Respawn mario");
            }
        }
    }
}



void SuperMarioSceneSingle::resetSession() {
    // 对象全清（含上一局的马里奥与静态场景），随后重建静态场景
    clearObjects();
    initStaticObjects();
    // 动态对象守卫复位：据此重新生成马里奥
    is_initDynamicObjects = false;
    show_death_screen = false;
    initDynamicObjects();
}

void SuperMarioSceneSingle::clearObjects() {
    game_objects.clear();
    game_objects_map.clear();
    collisionSystem = std::make_unique<CollisionSystem>();   // 顺带清空碰撞体引用
}

void SuperMarioSceneSingle::showDeathScreen(eng::Renderer& renderer) {
    const eng::Vec2u win = renderer.getSize();
    const float w = static_cast<float>(win.x);
    const float h = static_cast<float>(win.y);

    // 死亡屏固定屏幕坐标系：切默认视图，画完恢复原相机（原 sf::View 保存/恢复逻辑）
    const eng::Renderer::CameraState oldCamera = renderer.getCamera();
    renderer.resetCamera();

    renderer.drawRect(eng::FloatRect(0.f, 0.f, w, h), eng::Color(0, 0, 0, 180));

    const eng::FontHandle font = AssetManager::getInstance().getFontHandle();
    const eng::Vec2f diedSize = renderer.measureText(font, "YOU DIED", 64);
    renderer.drawText(font, "YOU DIED",
                      eng::Vec2f(w / 2.f - diedSize.x / 2.f, h * 0.3f - diedSize.y / 2.f),
                      64, eng::Color::Red);

    const eng::Vec2f hintSize = renderer.measureText(font, "Press R to Respawn    Press Esc to Quit", 24);
    renderer.drawText(font, "Press R to Respawn    Press Esc to Quit",
                      eng::Vec2f(w / 2.f - hintSize.x / 2.f, h * 0.55f),
                      24, eng::Color::White);

    renderer.setCamera(oldCamera);
}
#endif
