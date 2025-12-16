//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "Component.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <iostream>

class GameObject {
    friend class MoveComponent;
public:
    GameObject() : position(0, 0), size(0, 0), speed(0, 0), active(true), started(false) {
        this->id = idCounter++;
    }
    GameObject(float posX, float posY, float width, float height) {
        this->position = sf::Vector2f(posX, posY);
        this->size = sf::Vector2f(width, height);
        this->speed = sf::Vector2f(0, 0);
        this->started = false;
        this->active = true;
        this->id = idCounter++;
    }
    virtual ~GameObject() = default;
    virtual void handleEvent(sf::Event& e) = 0;
    virtual void update(sf::Time deltaTime) = 0;
    virtual void render(sf::RenderWindow* window) = 0;
    virtual void start() {
        started = true;
    }
    bool isActive() const {
        return active;
    }
    void setActive(const bool state) {
        active = state;
    }
    bool hasStarted() const {
        return started;
    }
    template <typename T, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[typeid(T).hash_code()] = component;
        component->setOwner(this);
        return component;
    }

    template <typename T, bool Start, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[typeid(T).hash_code()] = component;
        component->setOwner(this);
        if constexpr (Start) component->start();
        return component;
    }

    template <typename IT, typename T, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[typeid(IT).hash_code()] = component;
        component->setOwner(this);
        return component;
    }

    template <typename IT, typename T, bool Start, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[typeid(IT).hash_code()] = component;
        component->setOwner(this);
        if constexpr (Start) component->start();
        return component;
    }

    template <typename T>
    std::shared_ptr<T> getComponent() {
        if (components.find(typeid(T).hash_code()) != components.end()) {
            return std::static_pointer_cast<T>(components[typeid(T).hash_code()]);
        }
        std::cout << "Component not found: " << typeid(T).name() << std::endl;
        return nullptr;
    }

    template <typename IT, typename T>
    std::shared_ptr<T> getComponent() {
        if (components.find(typeid(IT).hash_code()) != components.end()) {
            return std::static_pointer_cast<T>(components[typeid(IT).hash_code()]);
        }
        std::cout << "Component not found: " << typeid(IT).name() << std::endl;
        return nullptr;
    }

    template <typename T>
    bool removeComponent() {
        if (components.find(typeid(T).hash_code()) != components.end()) {
            components.erase(typeid(T).hash_code());
            return true;
        }
        return false;
    }

    const sf::Vector2f& getPosition() const {
        return position;
    }

    const sf::Vector2f& getSize() const {
        return size;
    }

    const sf::Vector2f& getSpeed() const {
        return speed;
    }

    const std::string& getTag() const {
        return tag;
    }

    bool getMoveAble() const {
        return moveAble;
    }

    void setSize(float width, float height) {
        size = sf::Vector2f(width, height);
    }

    void updateComponents(sf::Time deltaTime) {
        for (const auto&[fst, snd] : components) {
            snd->update(deltaTime);
        }
    }

    void renderComponents(sf::RenderWindow* window) {
        for (const auto&[fst, snd] : components) {
            snd->render(window);
        }
    }

    void handleComponents(sf::Event& e) {
        for (const auto&[fst, snd] : components) {
            snd->handleEvent(e);
        }
    }

protected:
    virtual void setPosition(const float posX, const float posY) {
        position = sf::Vector2f(posX, posY);
    }

    sf::Vector2f position;
    sf::Vector2f size;
    sf::Vector2f speed;
    bool active;
    bool moveAble{true};
    bool started;
    int id;
    std::string tag = "game_object:";
    std::unordered_map<size_t, std::shared_ptr<Component>> components;
    inline static int idCounter = 0;
};


