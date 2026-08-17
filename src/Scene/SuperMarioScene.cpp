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

void SuperMarioScene::init() {
    Scene::init();
    this->setNetworkManager(&(this->simple_network));
    simple_network.setCurrentScene(this);
    if (is_init) return;
    is_init = true;
    collisionSystem = std::make_unique<CollisionSystem>();
#ifndef SERVER_BUILD
    bg.setTexture(AssetManager::getInstance().getTexture("level_1"));
    const float bg_scale = static_cast<float>(window->getSize().y) / bg.getLocalBounds().height;
    bg.setScale(bg_scale, bg_scale);

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

std::shared_ptr<GameObject> SuperMarioScene::spawnEntityWithNetwork(sf::Packet& packet) {
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
        fire_ball->getComponent<MoveComponent>()->setSpeed(sf::Vector2f(s_x, s_y));
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
void SuperMarioScene::render(sf::RenderWindow* _window) {
    _window->draw(bg);
    Scene::render(_window);
    if (show_death_screen) {
        showDeathScreen(_window);
    }
}
#endif

void SuperMarioScene::update(sf::Time deltaTime) {
    Scene::update(deltaTime);
    if (this->collisionSystem) {
        this->collisionSystem->checkCollisions();
    }
    simple_network.update(deltaTime);
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
void SuperMarioScene::handleEvent(sf::Event& event) {
    simple_network.handleEvent(event);

    if (camera) camera->handleEvent(event);
    // 必须用这种 for 循环，因为 game_objects 可能会改变，扩容导致迭代器失效
    for (int i = 0; i < game_objects.size(); ++i) {
        const auto& obj = game_objects[i];
        obj->handleEvent(event);
    }

    if (event.type == sf::Event::MouseButtonPressed) {
        const sf::Vector2i pos = getMousePosition();
        LOG_TRACE_FMT("Mouse clicked at ({}, {})", pos.x, pos.y);
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            getSceneManager()->loadScene("MenuScene");
        } else if (event.key.code == sf::Keyboard::R && show_death_screen) {
            show_death_screen = false;
            if (simple_network.getNetworkType() == NetworkManager::NetworkType::Server) {
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
void SuperMarioScene::showDeathScreen(sf::RenderWindow* _window) {
    // TODO: 优化性能
    sf::RectangleShape deathOverlay;
    sf::Text deathText;
    sf::Text deathHint;

    deathOverlay.setFillColor(sf::Color(0, 0, 0, 180));
    deathText.setFont(AssetManager::getInstance().getFont());
    deathText.setString("YOU DIED");
    deathText.setCharacterSize(64);
    deathText.setFillColor(sf::Color::Red);
    deathText.setStyle(sf::Text::Bold);
    deathHint.setFont(AssetManager::getInstance().getFont());
    deathHint.setString("Press R to Respawn    Press Esc to Quit");
    deathHint.setCharacterSize(24);
    deathHint.setFillColor(sf::Color::White);


    sf::View oldView = _window->getView();
    sf::View screenView(sf::FloatRect(0, 0,
        static_cast<float>(_window->getSize().x),
        static_cast<float>(_window->getSize().y)));
    _window->setView(screenView);

    const float w = static_cast<float>(_window->getSize().x);
    const float h = static_cast<float>(_window->getSize().y);
    deathOverlay.setSize(sf::Vector2f(w, h));
    _window->draw(deathOverlay);

    deathText.setPosition(
        w / 2.f - deathText.getGlobalBounds().width / 2.f,
        h * 0.3f - deathText.getGlobalBounds().height / 2.f);
    _window->draw(deathText);

    deathHint.setPosition(
        w / 2.f - deathHint.getGlobalBounds().width / 2.f,
        h * 0.55f);
    _window->draw(deathHint);

    _window->setView(oldView);
}
#endif
