//
// Created by MINEC on 2026/6/2.
//

#include "SuperMarioSceneMultiplayer.h"
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
#include "Goomba.h"
#include "Mushroom.h"
#include "Bowser.h"
#include "BowserFire.h"
#include "BowserAxe.h"
#include "Collision.h"
#include "Core/Types.h"
#include "MapLoader.h"
#ifndef SERVER_BUILD
#include "Render/Renderer.h"
#endif

void SuperMarioSceneMultiplayer::init() {
    Scene::init();
    this->setNetworkManager(&(this->simple_network));
    simple_network.setCurrentScene(this);
    if (is_init) {
        // 重进场景：清上一局会话（对象/碰撞/网络状态/提示标志）
        resetSession();
        return;
    }
    is_init = true;
    collisionSystem = std::make_unique<CollisionSystem>();
#ifndef SERVER_BUILD
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
#endif
    initStaticObjects();

#ifdef SERVER_BUILD
    startServer();
#endif
}

void SuperMarioSceneMultiplayer::exit() {
    Scene::exit();
    this->setNetworkManager(nullptr);
}

std::shared_ptr<GameObject> SuperMarioSceneMultiplayer::spawnEntity() {
    auto obj = std::make_shared<Mario>(100.f, 100.f, false);
    this->addObjectWithMap(obj);
    LOG_DEBUG_FMT("Create mario with id:{}", obj->getId());
    return obj;
}

std::shared_ptr<GameObject> SuperMarioSceneMultiplayer::spawnEntityWithNetwork() {
    auto obj = std::make_shared<Mario>(100.f, 100.f, false);
    this->addObjectWithNetwork(obj);
    LOG_DEBUG_FMT("Create mario with id:{}", obj->getId());
    return obj;
}

std::shared_ptr<GameObject> SuperMarioSceneMultiplayer::spawnEntityWithNetwork(eng::Packet& packet) {
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
    if (obj_type == ObjectType::Goomba) {
        float x, y, s_x;
        packet >> x >> y >> s_x;
        const auto goomba = std::make_shared<Goomba>(x, y, s_x);
        goomba->setId(id);
        LOG_DEBUG_FMT("Create goomba, id:{}, x:{}, y:{}, s_x:{}", id, x, y, s_x);
        this->addObjectWithNetwork(goomba);
        return goomba;
    }
    if (obj_type == ObjectType::Mushroom) {
        float x, y, s_x, birth_y;
        bool emerging, eaten;
        packet >> x >> y >> s_x >> birth_y >> emerging >> eaten;
        const auto mushroom = std::make_shared<Mushroom>(x, y, s_x);
        mushroom->setId(id);
        // 按服务端权威状态还原：新客户端加入时，已长出/已被吃的蘑菇不重放升起动画与音效
        mushroom->restoreNetworkState(birth_y, emerging, eaten);
        LOG_DEBUG_FMT("Create mushroom, id:{}, x:{}, y:{}, s_x:{}, emerging:{}, eaten:{}", id, x, y, s_x, emerging, eaten);
        this->addObjectWithNetwork(mushroom);
        // 渲染顺序重排：把蘑菇插到出生点正下方的方块之前，升起过程被方块遮挡（与单机一致）。
        // 不能按坐标精确匹配：客户端顶砖是本地预测，方块正在弹跳（y 每帧变化），
        // 收到 SpawnObject 时方块当前位置与出生快照对不上，所以按 x 相同 + y 最近的方块认领。
        auto& objs = this->getGameObjects();
        size_t mush_index = objs.size();
        for (size_t i = 0; i < objs.size(); ++i) {
            if (objs[i] == mushroom) {
                mush_index = i;
                break;
            }
        }
        size_t best_index = objs.size();
        float best_distance = 0.f;
        for (size_t i = 0; i < mush_index; ++i) {
            const auto& obj_ptr = objs[i];
            if (obj_ptr->getClassName() != "Box" || obj_ptr->getPosition().x != x) continue;
            const float diff = obj_ptr->getPosition().y - (y + 20.f);
            const float distance = diff < 0 ? -diff : diff;
            if (distance <= 64.f && (best_index == objs.size() || distance < best_distance)) {
                best_index = i;
                best_distance = distance;
            }
        }
        if (best_index < mush_index) {
            objs.erase(objs.begin() + mush_index);
            objs.insert(objs.begin() + best_index, mushroom);
        }
        return mushroom;
    }
    if (obj_type == ObjectType::Bowser) {
        float x, y, patrol_speed;
        bool activated, breathing, killed;
        int health;
        packet >> x >> y >> patrol_speed >> activated >> breathing >> health >> killed;
        const auto bowser = std::make_shared<Bowser>(x, y, patrol_speed);
        bowser->setId(id);
        // 按服务端权威状态还原：新客户端加入时不重放激活/死亡动画与音效
        bowser->restoreNetworkState(activated, breathing, health, killed);
        LOG_DEBUG_FMT("Create bowser, id:{}, x:{}, y:{}, activated:{}, health:{}, killed:{}", id, x, y, activated, health, killed);
        this->addObjectWithNetwork(bowser);
        return bowser;
    }
    if (obj_type == ObjectType::BowserFire) {
        float x, y, s_x;
        packet >> x >> y >> s_x;
        const auto bowser_fire = std::make_shared<BowserFire>(x, y, s_x);
        bowser_fire->setId(id);
        // 构造函数按"嘴部中心点"做了半高偏移，快照 y 是已偏移后的权威位置，
        // 重置回快照值避免二次偏移
        bowser_fire->getComponent<MoveComponent>()->setPosition(eng::Vec2f(x, y));
        LOG_DEBUG_FMT("Create bowser fire, id:{}, x:{}, y:{}, s_x:{}", id, x, y, s_x);
        this->addObjectWithNetwork(bowser_fire);
        return bowser_fire;
    }
    if (obj_type == ObjectType::BowserAxe) {
        float x, y, s_x;
        packet >> x >> y >> s_x;
        const auto bowser_axe = std::make_shared<BowserAxe>(x, y, s_x);
        bowser_axe->setId(id);
        LOG_DEBUG_FMT("Create bowser axe, id:{}, x:{}, y:{}, s_x:{}", id, x, y, s_x);
        this->addObjectWithNetwork(bowser_axe);
        return bowser_axe;
    }
    LOG_ERROR("Invalid object type");
    return nullptr;
}

void SuperMarioSceneMultiplayer::initStaticObjects() {
    // 地图外置：静态地形（左墙/砖块/箱子/地面）从配置指定的关卡地图加载（服务端同样需要地形）
    MapLoader::loadStaticObjects(*this);
}

void SuperMarioSceneMultiplayer::initDynamicObjects() {
    if (is_initDynamicObjects) return;
    is_initDynamicObjects = true;
    const auto map_data = MapLoader::loadDynamicData();
#ifndef SERVER_BUILD
    // 玩家出生点取自 level_1.json
    const float spawn_x = map_data ? map_data->player_spawn_x : 100.f;
    const float spawn_y = map_data ? map_data->player_spawn_y : 100.f;
    std::shared_ptr<Mario> mario = std::make_shared<Mario>(spawn_x, spawn_y);
    this->addObjectWithNetwork(mario);
    LOG_DEBUG("Create mario");
#endif
    // 敌人由服务器（含客户端房主）从地图生成：addObjectWithNetwork 会注册进
    // NetworkManager 并广播 SpawnObject；纯客户端不进入本函数，靠消息生成
    if (map_data) {
        for (const auto& spawn : map_data->goombas) {
            this->addObjectWithNetwork(std::make_shared<Goomba>(spawn.x, spawn.y, spawn.speed));
        }
        LOG_DEBUG_FMT("Created {} goombas", map_data->goombas.size());
        if (map_data->has_bowser) {
            this->addObjectWithNetwork(
                std::make_shared<Bowser>(map_data->bowser.x, map_data->bowser.y, map_data->bowser.speed));
            LOG_DEBUG("Created bowser");
        }
    }
}

#ifndef SERVER_BUILD
void SuperMarioSceneMultiplayer::render(eng::Renderer& _renderer) {
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

void SuperMarioSceneMultiplayer::update(eng::Time deltaTime) {
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

void SuperMarioSceneMultiplayer::addObject(const std::shared_ptr<GameObject>& obj) {
    Scene::addObject(obj);
    if (this->collisionSystem && obj->getComponent<Collision>()) {
        this->collisionSystem->addObject(obj);
    }
}

void SuperMarioSceneMultiplayer::addObjectWithNetwork(const std::shared_ptr<GameObject>& obj) {
    addObjectWithMap(obj);
    simple_network.addGameObjectAndSync(obj);
}

#ifndef SERVER_BUILD
void SuperMarioSceneMultiplayer::handleEvent(const eng::EngineEvent& event) {
    simple_network.handleEvent(event);

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

void SuperMarioSceneMultiplayer::startServer() {
    if (simple_network.startServer()) {
        initDynamicObjects();
    }
}

void SuperMarioSceneMultiplayer::resetSession() {
    clearObjects();
    initStaticObjects();
    // 动态对象守卫复位：单机/服务端路径据此重新生成马里奥
    is_initDynamicObjects = false;
    show_death_screen = false;
    show_disconnect_screen = false;
}

void SuperMarioSceneMultiplayer::clearObjects() {
    // 网络会话（连接断开/同步表/标志；Local 与 None 无连接资源，仅清表）
    simple_network.resetSession();
    // 对象全清（含上一局的马里奥与静态场景），随后重建静态场景
    game_objects.clear();
    game_objects_map.clear();
    collisionSystem = std::make_unique<CollisionSystem>();   // 顺带清空碰撞体引用
}

void SuperMarioSceneMultiplayer::connectToServer(const std::string& address) {
    simple_network.connectToServer(address);
}

#ifndef SERVER_BUILD
void SuperMarioSceneMultiplayer::showDeathScreen(eng::Renderer& renderer) {
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

void SuperMarioSceneMultiplayer::showDisconnectScreen(eng::Renderer& renderer) {
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
