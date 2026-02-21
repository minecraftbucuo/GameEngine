//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "BoxCollisionHandle.h"
#include "GameObject.h"
#include "BoxGameObject.h"

class Ground : public BoxGameObject {
public:
    Ground(const float x, const float y, const float width, const float height, const std::string& tag = "ground")
            : BoxGameObject(x, y, width, height) {
        this->tag = tag + ":" + std::to_string(id);
        this->moveAble = false;
        className = "Ground";
    }

    void setPosition(const float posX, const float posY) override {
        this->position = sf::Vector2f(posX, posY);
        const auto boxCollision = this->getComponent<Collision, BoxCollision>();
        boxCollision->setPosition(posX, posY);
    }
};


