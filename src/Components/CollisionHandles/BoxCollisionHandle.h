//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include "CollisionHandle.h"
#include "GameObject.h"
#include "BoxCollision.h"
#include "CircleCollision.h"

class BoxCollisionHandle : public CollisionHandle {
public:
    BoxCollisionHandle() {
        collisionHandlers[typeid(BoxCollision).hash_code()] = [this](const CollisionEvent& event) {
            this->handleCollisionWithBox(event);
        };
        collisionHandlers[typeid(CircleCollision).hash_code()] = [this](const CollisionEvent& event) {
            this->handleCollisionWithCircle(event);
        };
    }
    // 目前两种处理都是一样的
    void handleCollisionWithBox(const CollisionEvent &event) override {
        handle(event);
    }

    void handleCollisionWithCircle(const CollisionEvent &event) override {
        handle(event);
    }
};
