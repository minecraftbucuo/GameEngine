//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "GameScene.h"
#include <memory>
#include "Player.h"
#include "Controller.h"
#include "CameraComponent.h"
#include "GravityComponent.h"
#include "BoxGameObject.h"
#include "Ground.h"
#include "CollisionSystem.h"
#include "Circle.h"
#include "SceneManager.h"
#include "MoveComponent.h"
#include "Collision.h"

void GameScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;
    collisionSystem = std::make_unique<CollisionSystem>();
    initScene();
}

void GameScene::initScene() {
    std::shared_ptr<Player> player = std::make_shared<Player>(300, 0, 40);
    player->addComponent<Controller>();
    player->addComponent<CameraComponent>();
    this->addObject(player);

    std::shared_ptr<Player> player2 = std::make_shared<Player>(60, 300, 40);
    // player2->removeComponent<GravityComponent>();
    player2->getComponent<GravityComponent>()->setActive(false);
    // player2->addComponent<CameraComponent>();
    this->addObject(player2);


    std::shared_ptr<BoxGameObject> box = std::make_shared<BoxGameObject>(800, 800, 300, 80);
    const auto move = box->addComponent<MoveComponent>();
    move->setSpeedX(-200);
    // box->addComponent<GravityComponent>();
    this->addObject(box);

    // 左墙
    std::shared_ptr<Ground> wall1 = std::make_shared<Ground>(-200, 0, 10 + 200, 960, "wall1");
    this->addObject(wall1);
    // 右墙
    std::shared_ptr<Ground> wall2 = std::make_shared<Ground>(1190, 0, 10 + 200, 960, "wall2");
    this->addObject(wall2);
    // 天花板
    std::shared_ptr<Ground> wall3 = std::make_shared<Ground>(0, 0, 1200, 10, "wall3");
    this->addObject(wall3);
    //
    std::shared_ptr<Ground> ground = std::make_shared<Ground>(-10000, 940, 120000, 800);
    this->addObject(ground);
}

void GameScene::update(eng::Time deltaTime) {
    Scene::update(deltaTime);
    if (this->collisionSystem) {
        this->collisionSystem->checkCollisions();
    }
}

void GameScene::addObject(const std::shared_ptr<GameObject>& obj) {
    Scene::addObject(obj);
    if (this->collisionSystem && obj->getComponent<Collision>()) {
        this->collisionSystem->addObject(obj);
    }
}

void GameScene::handleEvent(sf::Event& event) {
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
        const auto mouse_position = getMousePosition();
        addObject(std::make_shared<Circle>(mouse_position.x - 20, mouse_position.y - 20, 20.f));
    } else if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            getSceneManager()->loadScene("MenuScene");
        }
    }
}

CollisionSystem* GameScene::getCollisionSystem() const {
    return collisionSystem.get();
}
#endif