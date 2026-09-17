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
#include "Bowser.h"
#include "Collision.h"
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
    // 左墙
    std::shared_ptr<Ground> wall1 = std::make_shared<Ground>(0, 0, 10, CONFIG.window.height, "wall1");
    this->addObject(wall1);

    std::vector<std::pair<int, int>> bricks = {
        {1154, 609}, {1429, 609}, {1557, 609}, {1621, 609},
        {13186, 571}
    };

    this->addObject(std::make_shared<Box>(1493, 609));

    for (const auto& [x, y] : bricks) {
        this->addObject(std::make_shared<Brick>(x, y));
    }

    std::vector<std::array<int, 4>> collisions = {
        {1927, 722, 2053, 852}, {2612, 655, 2738, 853}, {3163, 586, 3287, 851}, {3914, 585, 4042, 848},
        {9188, 789, 9256, 854}, {9256, 721, 9321, 853}, {9325, 651, 9389, 851}, {9395, 583, 9459, 851},
        {9599, 584, 9664, 851}, {9668, 651, 9734, 852}, {9738, 721, 9803, 853}, {9805, 790, 9873, 852},
        {10149, 789, 10212, 852}, {10217, 720, 10282, 853}, {10284, 651, 10352, 852}, {10355, 584, 10490, 852},
        {10629, 585, 10694, 851}, {10698, 652, 10761, 852}, {10764, 720, 10832, 850}, {10834, 789, 10901, 853},
        {11183, 723, 11310, 852}, {12280, 720, 12406, 853},
        {12412, 789, 12478, 852}, {12481, 720, 12544, 851}, {12547, 651, 12615, 852}, {12617, 583, 12682, 850},
        {12687, 513, 12752, 852}, {12755, 448, 12819, 850}, {12824, 378, 12889, 851}, {12892, 310, 13025, 853},
        {0, 857, 4728, 2000}, {4870, 858, 5893, 2000}, {6104, 859, 10489, 2000}, {10628, 857, 14535, 2000}
    };

    for (const auto [x1, y1, x2, y2] : collisions) {
        this->addObject(std::make_shared<Ground>(x1, y1, x2 - x1, y2 - y1));
    }
}

void SuperMarioSceneSingle::initDynamicObjects() {
    if (is_initDynamicObjects) return;
    is_initDynamicObjects = true;
    std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
    this->addObjectWithMap(mario);
    LOG_DEBUG("Create mario");

    // 生成板栗仔敌人：地面顶部 y=857，板栗仔高 64 → y=793
    const float goomba_y = 857.f - CONFIG.game.defaultBlockSize;
    this->addObject(std::make_shared<Goomba>(1800.f, goomba_y));
    this->addObject(std::make_shared<Goomba>(2800.f, goomba_y, 120.f));
    this->addObject(std::make_shared<Goomba>(5200.f, goomba_y));
    this->addObject(std::make_shared<Goomba>(8800.f, goomba_y, 120.f));
    this->addObject(std::make_shared<Goomba>(11500.f, goomba_y));

    // 乌龟大王 BOSS：最终楼梯后的平地（x≈13500），2 块砖高（128px）→ y = 857 - 128
    this->addObject(std::make_shared<Bowser>(1800.f,
                                             857.f - CONFIG.game.defaultBlockSize * 2.f));
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
        LOG_TRACE_FMT("Mouse clicked at ({}, {})", pos.x, pos.y);
    } else if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::Escape) {
            getSceneManager()->loadScene("MenuScene");
            clearObjects();
        } else if (event.key == eng::Key::R && show_death_screen) {
            show_death_screen = false;
            // 本地权威：死亡后 R 直接重生（与原 Server/Local 分支等价，网络同步为空操作）
            std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
            this->addObjectWithMap(mario);
            LOG_DEBUG("Respawn mario");
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
