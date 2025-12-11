//
// Created by MINEC on 2025/12/10.
//

#pragma once
#include <vector>
#include <memory>
#include "WorldContext.h"
#include "CollisionSystem.h"
#include "GameObject.h"
#include "BoxCollision.h"
#include "Controller.h"

class World {
public:
    World() : size(1200, 960), collisionSystem (nullptr) {}
    World(const float width, const float height) : size(width, height) {
        this->collisionSystem = new CollisionSystem();
    }
    ~World() = default;

    std::vector<std::shared_ptr<GameObject>>& getGameObjects() {
        return game_objects;
    }

    void handleEvent(sf::Event& event) {
        for (const auto& obj : game_objects) {
            obj->handleEvent(event);
        }
    }

    void update(const sf::Time deltaTime) {
        for (const auto& obj : game_objects) {
            if (obj->isActive()) {
                if (obj->hasStarted()) obj->update(deltaTime);
                else obj->start();
            }
        }
        if (this->collisionSystem) {
            this->collisionSystem->checkCollisions();
        }
    }

    void render(sf::RenderWindow* window) {
        for (const auto& obj : game_objects) {
            if (obj->isActive()) {
                obj->render(window);
            }
        }
    }

    void addObject(const std::shared_ptr<GameObject>& obj) {
        game_objects.push_back(obj);
        if (this->collisionSystem && obj->getComponent<Collision>()) {
            this->collisionSystem->addObject(obj);
        }
    }

    void setWorldContext() {
        WorldContext::getInstance().setWorldSize(size.x, size.y);
        for (const auto& obj : game_objects) {
            if (obj->tag == "ground") {
                obj->setPosition(0.f, size.y);
                obj->size.x = size.x;
                const auto boxCollision = obj->getComponent<Collision, BoxCollision>();
                boxCollision->setPosition(0.f, size.y);
                boxCollision->setSize(size.x, 2000.f);
                break;
            }
        }
    }

    void setWorldSize(const float width, const float height) {
        size.x = width;
        size.y = height;
    }

private:
    std::vector<std::shared_ptr<GameObject>> game_objects;
    sf::Vector2f size;
    CollisionSystem* collisionSystem;
};


