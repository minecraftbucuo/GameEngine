//
// Created by MINEC on 2026/1/2.
//

#pragma once
#include "BoxGameObject.h"
#include "CameraComponent.h"
#include "Controller.h"
#include "Ground.h"
#include "Player.h"
#include "Scene.h"
#include "CollisionSystem.h"


class GameScene : public Scene {
public:
    GameScene(sf::RenderWindow* _window) : Scene(_window) {}
    ~GameScene() override = default;

    void init() override {
        this->setCamera(window);
        this->setSceneContext();
        collisionSystem = std::make_unique<CollisionSystem>();
        initScene();
    }

    void initScene() {
        std::shared_ptr<Player> player = std::make_shared<Player>(300, 0, 40);
        player->addComponent<Controller>();
        player->addComponent<CameraComponent>();
        this->addObject(player);

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
        // 右墙
        std::shared_ptr<Ground> wall2 = std::make_shared<Ground>(1190, 0, 10, 960, "wall2");
        this->addObject(wall2);
        // 天花板
        std::shared_ptr<Ground> wall3 = std::make_shared<Ground>(0, 0, 1200, 10, "wall3");
        this->addObject(wall3);
        //
        std::shared_ptr<Ground> ground = std::make_shared<Ground>(-10000, 940, 120000, 80);
        this->addObject(ground);
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
                    std::cout << window->getSize().y << std::endl;
                    obj_ground->setPosition(0.f, window->getSize().y - 20.f);
                    break;
                }
            }
        }
    }

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
};
