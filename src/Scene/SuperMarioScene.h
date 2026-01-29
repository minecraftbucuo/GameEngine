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
        AnimationManager::getInstance().loadAnimation();
        initScene();
    }

    void initScene() {
        std::shared_ptr<Mario> mario = std::make_shared<Mario>(100.f, 100.f);
        this->addObject(mario);

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
                    obj_ground->setPosition(0.f, window->getSize().y - 20.f);
                    break;
                }
            }
        }
    }

private:
    std::unique_ptr<CollisionSystem> collisionSystem;
};
