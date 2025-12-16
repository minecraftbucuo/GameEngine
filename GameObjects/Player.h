//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <SFML/Graphics.hpp>
#include "GameObject.h"
#include "EventBus.h"
#include "Events.h"
#include <string>

#include "BoxCollisionHandle.h"
#include "CircleCollisionHandle.h"
#include "CollisionHandle.h"
#include "GravityComponent.h"
#include "MoveComponent.h"

class Player : public GameObject {
public:
    Player(const float x, const float y, const float radius, const std::string& tag = "player") {
        shape.setRadius(radius);
        this->position = sf::Vector2f(x, y);
        this->size = sf::Vector2f(radius * 2, radius * 2);
        shape.setPosition(x, y);

        this->addComponent<Collision, CircleCollision>(this->position.x + radius, this->position.y + radius, this->size.x / 2);
        this->addComponent<CollisionHandle, CircleCollisionHandle>();
        // this->addComponent<Collision, BoxCollision, true>();
        // this->addComponent<CollisionHandle, BoxCollisionHandle>();

        this->addComponent<MoveComponent>();
        this->addComponent<GravityComponent>();
        this->tag = tag + ":" + std::to_string(this->id);
    }
    ~Player() override = default;
    void update(sf::Time deltaTime) override {
        updateComponents(deltaTime);
    }
    void render(sf::RenderWindow* window) override {
        renderComponents(window);
        window->draw(shape);
    }
    void handleEvent(sf::Event& e) override {
        handleComponents(e);
    }
    void start() override {
        GameObject::start();
        EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
            [this](const CollisionEvent& collisionEvent) {
            if (const auto& handler = this->getComponent<CollisionHandle>()) {
                handler->handleCollision(collisionEvent);
            }
        });
    }

private:
    void setPosition(const float x, const float y) override {
        // std::cout << "Player setPosition:" << x << " " << y << std::endl;
        GameObject::setPosition(x, y);
        shape.setPosition(x, y);
    }

    void setSpeed(const float x, const float y) {
        this->speed.x = x;
        this->speed.y = y;
    }

    sf::CircleShape shape;
};


