//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "BoxCollisionHandle.h"
#include "GameObject.h"

class Ground : public GameObject {
public:
    Ground(const float x, const float y, const float width, const float height)
            : GameObject(x, y, width, height) {
        this->tag = "ground" + std::to_string(id);
        this->moveAble = false;
        this->addComponent<Collision, BoxCollision, true>();
        this->addComponent<CollisionHandle, BoxCollisionHandle>();
        EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
            [this](const CollisionEvent& event) {
                if (const auto &handler = this->getComponent<CollisionHandle>()) {
                    handler->handleCollision(event);
                }
            }
        );
    }
    void update(sf::Time deltaTime) override {
        for (const auto&[fst, snd] : components) {
            snd->update(deltaTime);
        }
    }
    void render(sf::RenderWindow* window) override {
        for (const auto&[fst, snd] : components) {
            // std::cout << "Ground component render" << std::endl;
            snd->render(window);
        }
    }
    void handleEvent(sf::Event& e) override {
        for (const auto&[fst, snd] : components) {
            snd->handleEvent(e);
        }
    }
    void setPosition(const float posX, const float posY) override {
        this->position = sf::Vector2f(posX, posY);
    }
    void start() override {
        GameObject::start();
    }

    void setTag(const std::string& tag) {
        this->tag = tag;
    }
};


