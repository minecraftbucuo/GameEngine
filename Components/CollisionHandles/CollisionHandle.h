//
// Created by MINEC on 2025/12/11.
//

#pragma once
#include "Component.h"
#include "Events.h"

class CollisionHandle : public Component {
public:
    void start() override {}
    void update(sf::Time deltaTime) override {}
    void render(sf::RenderWindow* window) override {}
    void handleEvent(sf::Event& event) override {}

    void handleCollision(const CollisionEvent& event) {
        size_t hash_code = typeid(*event.b->getComponent<Collision>()).hash_code();
        if (this->collisionHandlers.find(hash_code) == this->collisionHandlers.end()) {
            std::cout << "error: no match collision handler for " << typeid(*event.b->getComponent<Collision>()).name() << std::endl;
            return;
        }
        this->collisionHandlers[hash_code](event);
    }

    virtual void handleCollisionWithBox(const CollisionEvent& event) = 0;
    virtual void handleCollisionWithCircle(const CollisionEvent& event) = 0;

protected:
    std::unordered_map<size_t, std::function<void(CollisionEvent)>> collisionHandlers;
};