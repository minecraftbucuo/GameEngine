//
// Created by MINEC on 2025/12/13.
//

#pragma once

#include "BoxCollisionHandle.h"
#include "EventBus.h"
#include "GameObject.h"

class BoxGameObject : public GameObject {
public:
    BoxGameObject() = default;
    BoxGameObject(const float posX, const float posY, const float width, const float height, const std::string& tag = "box")
            : GameObject(posX, posY, width, height) {
        this->tag = tag + ":" + std::to_string(id);
        this->addComponent<Collision, BoxCollision, true>();
        this->addComponent<CollisionHandle, BoxCollisionHandle>();
    }

    void start() override {
        GameObject::start();
        EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
            [this](const CollisionEvent& event) {
                if (const auto &handler = this->getComponent<CollisionHandle>()) {
                    handler->handleCollision(event);
                }
            }
        );
    }

};

