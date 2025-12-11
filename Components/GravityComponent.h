//
// Created by MINEC on 2025/12/9.
//

#pragma once

#include "Component.h"
#include "GameObject.h"
#include "WorldContext.h"

class GravityComponent : public Component {
public:
    GravityComponent() = default;
    void start() override {}
    void update(sf::Time deltaTime) override {
        float worldHeight = WorldContext::getInstance().getWorldHeight();
        //std::cout << (abs(this->owner->posY + this->owner->height - worldHeight) < 0.1f) << std::endl;
        //std::cout << owner->speedY << std::endl;
        if (std::abs(this->owner->position.y + this->owner->size.y - worldHeight) < 0.1f && std::abs(owner->speed.y) <= 1.f) return;
//        owner->posY += owner->speedY;
        /*if (owner->posY + owner->height >= worldHeight) {
            owner->speedY = -owner->speedY / 1.4;
            if (abs(owner->speedY) <= 1) owner->speedY = 0;
            owner->posY = worldHeight - owner->height;
        }*/
        //owner->setPosition(owner->posX, owner->posY);
        owner->speed.y += gravity * deltaTime.asSeconds();
        //if (owner->posY + owner->height + 1 >= worldHeight && abs(owner->speedY) <= 1) owner->speedY = 0;
    }
    void render(sf::RenderWindow* window) override {

    }
    void handleEvent(sf::Event& event) override {

    }
    std::string getName() {
        return name;
    }
private:
    float gravity = 3200.f;
    std::string name = "GravityComponent";
};


