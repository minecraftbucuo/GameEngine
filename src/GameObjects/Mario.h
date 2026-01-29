//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "GameObject.h"
#include <SFML/Graphics.hpp>

#include "AnimationManager.h"
#include "AssetManager.h"
#include "MarioRunState.h"
#include "StateMachine.h"


class Mario : public GameObject {
public:
    Mario(const float x, const float y, const std::string& tag = "mario") {
        this->position = sf::Vector2f(x, y);
        this->addComponent<Collision, BoxCollision, true>();
        this->addComponent<CollisionHandle, CircleCollisionHandle>();
        this->addComponent<GravityComponent>();
        this->addComponent<MoveComponent>();
        this->addComponent<Controller>();
        const auto stateMachine = this->addComponent<StateMachine>();
        stateMachine->addState<MarioRunState>();
        stateMachine->setState("MarioRunState");

        this->tag = tag + ":" + std::to_string(this->id);
    }

    void start() override {
        GameObject::start();
        EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
            [this](const CollisionEvent& collisionEvent) -> void {
            if (const auto& handler = this->getComponent<CollisionHandle>()) {
                handler->handleCollision(collisionEvent);
            }
        });
    }
};
