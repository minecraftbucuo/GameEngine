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
        if (event.type == sf::Event::Resized) {
            this->setWorldSize(static_cast<float>(event.size.width), static_cast<float>(event.size.height));
            this->setWorldContext();
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
            if (obj->getTag().substr(0, 6) == "ground") {
                obj->setSize(size.x, size.y);
                obj->setPosition(0.f, size.y - 20.f);
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


