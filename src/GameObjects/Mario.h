//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "GameObject.h"
#include <SFML/Graphics.hpp>

#include "AnimationManager.h"
#include "AssetManager.h"
#include "MarioController.h"
#include "MarioIdleState.h"
#include "MarioJumpState.h"
#include "MarioRunState.h"
#include "StateMachine.h"
#include "SceneContext.h"


class Mario : public GameObject {
public:
    Mario(const float x, const float y, const std::string& tag = "mario") {
        this->position = sf::Vector2f(x, y);

        this->addComponent<MarioController>();

        this->addComponent<Collision, BoxCollision, true>();
        // this->addComponent<CollisionHandle, BoxCollisionHandle>();
        this->addComponent<GravityComponent>();
        this->addComponent<MoveComponent>();
        this->addComponent<CameraComponent>();

        const auto stateMachine = this->addComponent<StateMachine>();
        stateMachine->addState<MarioRunState>();
        stateMachine->addState<MarioIdleState>();
        stateMachine->addState<MarioJumpState>();
        stateMachine->setState("MarioRunState");

        this->tag = tag + ":" + std::to_string(this->id);
    }

    void start() override {
        GameObject::start();
        EventBus::getInstance().subscribe<CollisionEvent>(
            "onCollision" + this->tag,
            [this](const CollisionEvent& collisionEvent) -> void {
                handleCollision(collisionEvent);
            }
        );
    }

    void update(sf::Time deltaTime) override {
        GameObject::update(deltaTime);
        // std::cout << this->getSpeed().y << std::endl;
        // std::cout << this->getComponent<StateMachine>()->getCurrentStateName() << std::endl;
        if (needGravity()) {
            this->getComponent<GravityComponent>()->setActive(true);
        }
    }

    bool needGravity() {
        auto collision = this->getComponent<Collision>();
        sf::Vector2f dy = sf::Vector2f(0.f, 1.f);
        collision->setPosition(collision->getPosition() + dy);
        const auto game_objects = *SceneContext::getInstance().getGameObjects();
        for (auto& game_object : game_objects) {
            if (game_object->getTag() == this->getTag()) continue;
            auto other_collision = game_object->getComponent<Collision>();
            if (!other_collision) continue;
            if (other_collision->checkCollision(*collision)) {
                collision->setPosition(collision->getPosition() - dy);
                return false;
            }
        }
        collision->setPosition(collision->getPosition() - dy);
        return true;
    }

    void handleCollision(const CollisionEvent& event) {
        auto& this_ = event.a;
        auto& other = event.b;

        // std::cout << this_->getTag() << ' ' << other->getTag() << std::endl;

        if (!this_->getMoveAble()) return;
        std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
        if (!moveComponent) return;

        // 计算 x 方向和 y 方向的重合度
        const float dx = std::min(event.a_position.x + this_->getSize().x,
            event.b_position.x + other->getSize().x) - std::max(event.a_position.x, event.b_position.x);
        const float dy = std::min(event.a_position.y + this_->getSize().y,
            event.b_position.y + other->getSize().y) - std::max(event.a_position.y, event.b_position.y);

        if (std::abs(dx - dy) <= 0.1f) return;
        // 水平碰撞
        if (dx < dy) {
            // const float relativeSpeedX = event.b_speed.x - event.a_speed.x;
            // moveComponent->setSpeedX(relativeSpeedX * 0.28f);
            // if (std::abs(this_->getSpeed().x) <= 2.f) {
            //     moveComponent->setSpeedX(0.f);
            // }
            float right_x = std::abs(event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
            float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
            if (right_x < left_x) {
                moveComponent->setPositionX(event.b_position.x - this_->getSize().x);
            } else {
                moveComponent->setPositionX(event.b_position.x + other->getSize().x);
            }
        } else {
            const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
            moveComponent->setSpeedY(relativeSpeedY * 0.28f);
            if (std::abs(this_->getSpeed().y) <= 2.f) {
                moveComponent->setSpeedY(0.f);
            }
            float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
            float bottom_y = std::abs(event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
            if (top_y > bottom_y) {
                moveComponent->setPositionY(event.b_position.y - this_->getSize().y);
                moveComponent->setSpeedY(0.f);
                this->getComponent<StateMachine>()->setState("MarioIdleState");
                this->getComponent<GravityComponent>()->setActive(false);
            } else {
                moveComponent->setPositionY(event.b_position.y + other->getSize().y);
            }
        }
    }
};
