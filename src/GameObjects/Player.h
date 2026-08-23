//
// Created by MINEC on 2025/12/9.
//


#pragma once
#ifndef SERVER_BUILD
#include "GameObject.h"
#include "EventBus.h"
#include <string>

namespace eng { class Renderer; }

class Player : public GameObject {
public:
    Player(float x, float y, float radius, const std::string& tag = "player");

    ~Player() override {
        EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
    }

    // SDL3 迁移 6e：sf::CircleShape 数据化（position/size 基类已有，半径 = size.x/2）
    void render(eng::Renderer& renderer) override {
        renderer.drawCircle(position + size * 0.5f, size.x * 0.5f, eng::Color::White);
        renderComponents(renderer);
    }

    void start() override;

private:
    void setPosition(const float x, const float y) override {
        // std::cout << "Player setPosition:" << x << " " << y << std::endl;
        GameObject::setPosition(x, y);
    }

    void setSpeed(const float x, const float y) {
        this->speed.x = x;
        this->speed.y = y;
    }
};

#endif
