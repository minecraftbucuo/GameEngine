//
// Created by MINEC on 2026/6/2.
//

#include "SuperMarioScene.h"
#include "AssetManager.h"
#include "EventBus.h"
#include "Mario.h"
#include "HealthBar.h"
#include "Ground.h"
#include "Box.h"
#include "Brick.h"
#include "SceneManager.h"
#include "MoveComponent.h"
#include "FireBall.h"
#include "Collision.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"
#endif

void SuperMarioScene::init() {
    Scene::init();
    this->setNetworkManager(&(this->simple_network));
    simple_network.setCurrentScene(this);
    // N4：每次进场景复位断线提示（is_init 守卫只护重资源，标志须逐次刷新，
    // 否则上一局的断线残留会在重连后的新对局里立刻弹出提示层）
    show_disconnect_screen = false;
    simple_network.clearConnectionLost();
    if (is_init) return;
    is_init = true;
    collisionSystem = std::make_unique<CollisionSystem>();
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：背景数据化——按窗口高度等比缩放铺满（原 sf::Sprite setScale 逻辑）
    bg_texture = AssetManager::getInstance().getTextureHandle("level_1");
    const eng::Vec2u tex_size = AssetManager::getInstance().getTextureSize(bg_texture);
    const eng::Vec2u win_size = renderer->getSize();
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
#endif
    initStaticObjects();

#ifdef SERVER_BUILD
    startServer();
#endif
}

void SuperMarioScene::exit() {
    Scene::exit();
    this->setNetworkManager(nullptr);
}

std::shared_ptr<GameObject> SuperMarioScene::spawnEntity() {
    auto obj = std::make_shared<Mario>(100.f, 100.f, false);
    this->addObjectWithMap(obj);
    LOG_DEBUG_FMT("Create mario with id:{}", obj->getId());
    return obj;
}

std::shared_ptr<GameObject> SuperMarioScene::spawnEntityWithNetwork() {
    auto obj = std::make_shared<Mario>(100.f, 100.f, false);
    this->addObjectWithNetwork(obj);
    LOG_DEBUG_FMT("Create mario with id:{}", obj->getId());
    return obj;
}

std::shared_ptr<GameObject> SuperMarioScene::spawnEntityWithNetwork(eng::Packet& packet) {
    unsigned int id;
    ObjectType obj_type;
    packet >> id >> obj_type;
    if (obj_type == ObjectType::MarioPlayer || obj_type == ObjectType::Mario) {
        float x, y, s_x, s_y;
        bool is_jump;
        int health;
        packet >> x >> y >> s_x >> s_y >> is_jump >> health;
        const auto player = std::make_shared<Mario>(x, y, obj_type == ObjectType::MarioPlayer);
        player->setId(id);
        LOG_DEBUG_FMT("Create mario, id:{}, x:{}, y:{}, s_x:{}, s_y:{}, is_jump:{}, health:{}", id, x, y, s_x, s_y, is_jump, health);
        const auto& move_component = player->getComponent<MoveComponent>();
        move_component->setSpeed(s_x, s_y);
        player->getComponent<HealthBar>()->setHealth(health);
        this->addObjectWithNetwork(player);
        return player;
    }
    if (obj_type == ObjectType::FireBall) {
        unsigned int owner_id;
        float x, y, s_x, s_y;
        packet >> owner_id >> x >> y >> s_x >> s_y;
        const auto fire_ball = std::make_shared<FireBall>(owner_id, x, y);
        fire_ball->setId(id);
        fire_ball->getComponent<MoveComponent>()->setSpeed(eng::Vec2f(s_x, s_y));
        this->addObjectWithNetwork(fire_ball);
        return fire_ball;
    }
    LOG_ERROR("Invalid object type");
    return nullptr;
}

void SuperMarioScene::initStaticObjects() {
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

void SuperMarioScene::initDynamicObjects() {
    if (is_initDynamicObjects) return;
    is_initDynamicObjects = true;
#ifndef SERVER_BUILD
    std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
    this->addObjectWithNetwork(mario);
    LOG_DEBUG("Create mario");
#endif
}

#ifndef SERVER_BUILD
void SuperMarioScene::render(eng::Renderer& _renderer) {
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
    if (show_disconnect_screen) {
        showDisconnectScreen(_renderer);
    }
}
#endif

void SuperMarioScene::update(eng::Time deltaTime) {
    Scene::update(deltaTime);
    if (this->collisionSystem) {
        this->collisionSystem->checkCollisions();
    }
    simple_network.update(deltaTime);
    // N4：断线一次即定格提示层，直到 ESC 回菜单（重进场景时复位）
    if (simple_network.wasConnectionLost()) {
        show_disconnect_screen = true;
    }
}

void SuperMarioScene::addObject(const std::shared_ptr<GameObject>& obj) {
    Scene::addObject(obj);
    if (this->collisionSystem && obj->getComponent<Collision>()) {
        this->collisionSystem->addObject(obj);
    }
}

void SuperMarioScene::addObjectWithNetwork(const std::shared_ptr<GameObject>& obj) {
    addObjectWithMap(obj);
    simple_network.addGameObjectAndSync(obj);
}

#ifndef SERVER_BUILD
void SuperMarioScene::handleEvent(const eng::EngineEvent& event) {
    simple_network.handleEvent(event);

    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        const auto& obj = game_objects[i];
        obj->handleEvent(event);
    }

    if (event.type == eng::EventType::MouseButtonPress) {
        const eng::Vec2i pos = getMousePosition();
        LOG_TRACE_FMT("Mouse clicked at ({}, {})", pos.x, pos.y);
    } else if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::Escape) {
            getSceneManager()->loadScene("MenuScene");
        } else if (event.key == eng::Key::R && show_death_screen) {
            show_death_screen = false;
            // WASM 移植 Step 5：Local（网页单机）与 Server 同为本地权威，允许直接重生
            const auto net_type = simple_network.getNetworkType();
            if (net_type == NetworkManager::NetworkType::Server
                || net_type == NetworkManager::NetworkType::Local) {
                std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
                this->addObjectWithNetwork(mario);
                LOG_DEBUG("Respawn mario");
            }
        }
    }
}
#endif

void SuperMarioScene::startServer() {
    if (simple_network.startServer()) {
        initDynamicObjects();
    }
}

void SuperMarioScene::connectToServer(const std::string& address) {
    simple_network.connectToServer(address);
}

#ifndef SERVER_BUILD
void SuperMarioScene::showDeathScreen(eng::Renderer& renderer) {
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

void SuperMarioScene::showDisconnectScreen(eng::Renderer& renderer) {
    const eng::Vec2u win = renderer.getSize();
    const float w = static_cast<float>(win.x);
    const float h = static_cast<float>(win.y);

    // 与死亡屏同构：屏幕坐标系遮罩，画完恢复相机（远端玩家已冻结、本地仍可移动，
    // 遮罩半透明保留视野，玩家明确知道该做什么：ESC 回菜单重连）
    const eng::Renderer::CameraState oldCamera = renderer.getCamera();
    renderer.resetCamera();

    renderer.drawRect(eng::FloatRect(0.f, 0.f, w, h), eng::Color(0, 0, 0, 180));

    const eng::FontHandle font = AssetManager::getInstance().getFontHandle();
    const eng::Vec2f lostSize = renderer.measureText(font, "CONNECTION LOST", 64);
    renderer.drawText(font, "CONNECTION LOST",
                      eng::Vec2f(w / 2.f - lostSize.x / 2.f, h * 0.3f - lostSize.y / 2.f),
                      64, eng::Color::Red);

    const eng::Vec2f hintSize = renderer.measureText(font, "Press Esc to return to Menu", 24);
    renderer.drawText(font, "Press Esc to return to Menu",
                      eng::Vec2f(w / 2.f - hintSize.x / 2.f, h * 0.55f),
                      24, eng::Color::White);

    renderer.setCamera(oldCamera);
}
#endif
