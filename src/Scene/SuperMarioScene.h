//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "BoxGameObject.h"
#include "Ground.h"
#include "Scene.h"
#include "CollisionSystem.h"
#include "Mario.h"


class SuperMarioScene : public Scene {
public:
    SuperMarioScene(sf::RenderWindow* _window) : Scene(_window) {}
    ~SuperMarioScene() override = default;

    void init() override {
        this->setCamera(window);
        this->setSceneContext();
        collisionSystem = std::make_unique<CollisionSystem>();
        AssetManager::getInstance().loadTexture("E:/Project/C++ Program/CLion/GameEngine/src/Asset/SuperMario/resources/graphics");
        FrameManager::getInstance().loadFrame();

        bg.setTexture(AssetManager::getInstance().getTexture("level_1"));
        const float bg_scale = static_cast<float>(window->getSize().y) / bg.getLocalBounds().height;
        bg.setScale(bg_scale, bg_scale);
        initScene();
    }

    void initScene() {
        std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
        this->addObject(mario);

        std::shared_ptr<Player> player2 = std::make_shared<Player>(60, 300, 40);
        player2->removeComponent<GravityComponent>();
        // player2->addComponent<CameraComponent>();
        this->addObject(player2);


        std::shared_ptr<BoxGameObject> box = std::make_shared<BoxGameObject>(800, 800, 300, 80);
        const auto move = box->addComponent<MoveComponent>();
        move->setSpeedX(-200);
        // box->addComponent<GravityComponent>();
        this->addObject(box);

        // 左墙
        std::shared_ptr<Ground> wall1 = std::make_shared<Ground>(0, 0, 10, 960, "wall1");
        this->addObject(wall1);
        // 地板
        std::shared_ptr<Ground> ground = std::make_shared<Ground>(-10000, 857, 120000, 80);
        this->addObject(ground);

        std::shared_ptr<Ground> pipe1 = std::make_shared<Ground>(1927, 720, 124, 137);
        this->addObject(pipe1);
    }

    void render(sf::RenderWindow* _window) override {
        _window->draw(bg);
        Scene::render(_window);
    }

    void update(sf::Time deltaTime) override {
        Scene::update(deltaTime);
        if (this->collisionSystem) {
            this->collisionSystem->checkCollisions();
        }
    }

    void addObject(const std::shared_ptr<GameObject>& obj) override {
        Scene::addObject(obj);
        if (this->collisionSystem && obj->getComponent<Collision>()) {
            this->collisionSystem->addObject(obj);
        }
    }

    void handleEvent(sf::Event& event) override {
        Scene::handleEvent(event);
        if (event.type == sf::Event::Resized) {
            for (const auto& obj : game_objects) {
                if (obj->getTag().substr(0, 6) == "ground") {
                    // obj->setSize(size.x, size.y);
                    const std::shared_ptr<Ground> obj_ground = std::dynamic_pointer_cast<Ground>(obj);
                    obj_ground->setPosition(0.f, window->getSize().y - 20.f);
                    break;
                }
            }
        } else if (event.type == sf::Event::MouseButtonPressed) {
            const sf::Vector2f pos = SceneContext::getInstance().getCamera()->getCenter();
            // std::cout << pos.x - 600 << " " << pos.y - 480 << std::endl;
            std::cout << pos.x - 600 + event.mouseButton.x << " " << pos.y - 480 + event.mouseButton.y << std::endl;
        }
    }

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
    sf::Sprite bg;
};
