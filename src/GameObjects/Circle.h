//
// Created by MINEC on 2026/2/4.
//

#pragma once
#include "GameObject.h"
#include <SFML/Graphics.hpp>

#include "FrameManager.h"
#include "AssetManager.h"
#include "CameraComponent.h"
#include "EventBus.h"
#include "Events.h"
#include "GravityComponent.h"
#include "MarioController.h"
#include "MarioJumpState.h"
#include "SceneContext.h"
#include "SceneManager.h"


class Circle : public GameObject {
public:
    Circle(const float x, const float y, const float radius, const std::string& tag = "circle") {
        shape.setRadius(radius);
        this->position = sf::Vector2f(x, y);
        this->size = sf::Vector2f(radius * 2, radius * 2);
        shape.setPosition(x, y);

        this->addComponent<Collision, CircleCollision>(this->position.x + radius, this->position.y + radius, this->size.x / 2);
        this->addComponent<CollisionHandle, CircleCollisionHandle>();
        this->addComponent<GravityComponent>();
        this->addComponent<MoveComponent>();

        this->tag = tag + ":" + std::to_string(this->id);
    }

    void render(sf::RenderWindow* window) override {
        renderComponents(window);
        window->draw(shape);
    }

    void start() override {
        GameObject::start();
        EventBus::getInstance().subscribe<CollisionEvent>(
            "onCollision" + this->tag,
            [this](const CollisionEvent& collisionEvent) {
                if (const auto& handler = this->getComponent<CollisionHandle>()) {
                    handler->handleCollision(collisionEvent);
                    this->getComponent<GravityComponent>()->setActive(false);
                }
            }
        );
    }

    void update(sf::Time deltaTime) override {
        if (needGravity()) {
            this->getComponent<GravityComponent>()->setActive(true);
        }
        GameObject::update(deltaTime);
    }

    bool needGravity() {
        auto collision = this->getComponent<Collision>();
        sf::Vector2f dy = sf::Vector2f(0.f, 1.f);
        collision->setPosition(collision->getPosition() + dy);

        const auto game_objects = *SceneContext::getInstance().
        getSceneManager()->getCurrentScene()->getCollisionSystem()->getObjects();

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

private:
    void setPosition(const float x, const float y) override {
        GameObject::setPosition(x, y);
        shape.setPosition(x, y);
    }

    void setSpeed(const float x, const float y) {
        this->speed.x = x;
        this->speed.y = y;
    }

    sf::CircleShape shape;
};


