//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <SFML/Graphics.hpp>

class GameObject;
class Component {
public:
    Component() = default;
    explicit Component(GameObject* owner) : owner(owner) {}
    virtual ~Component() = default;

    virtual void start() {}
    virtual void update(const sf::Time& deltaTime) {}
    virtual void render(sf::RenderWindow* window) {}
    virtual void handleEvent(const sf::Event& event) {}
    void setOwner(GameObject* obj) {
        owner = obj;
    }
    [[nodiscard]] GameObject* getOwner() const {
        return owner;
    }
    void setActive(const bool value) {
        active = value;
    }
    [[nodiscard]] bool getActive() const {
        return active;
    }

protected:
    GameObject* owner{};
    bool active = true;
};

