//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <SFML/Graphics.hpp>

class GameObject;
class Component {
public:
    virtual ~Component() = default;

    virtual void start() = 0;
    virtual void update(sf::Time deltaTime) = 0;
    virtual void render(sf::RenderWindow* window) = 0;
    virtual void handleEvent(sf::Event& event) = 0;
    void setOwner(GameObject* obj) {
        owner = obj;
    }
    [[nodiscard]] GameObject* getOwner() const {
        return owner;
    }

protected:
    GameObject* owner{};
};

