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
    friend class StateMachine;
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
    virtual void handleEvent(sf::Event& e) {
        handleComponents(e);
    }
    virtual void update(sf::Time deltaTime) {
        updateComponents(deltaTime);
    }
    virtual void render(sf::RenderWindow* window) {
        renderComponents(window);
    }
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
        const size_t componentId = typeid(T).hash_code();
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        return component;
    }

    template <typename T, bool Start, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        const size_t componentId = typeid(T).hash_code();
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        if constexpr (Start) component->start();
        return component;
    }

    template <typename IT, typename T, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        const size_t componentId = typeid(IT).hash_code();
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        return component;
    }

    template <typename IT, typename T, bool Start, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        const size_t componentId = typeid(IT).hash_code();
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        if constexpr (Start) component->start();
        return component;
    }

    template <typename T>
    std::shared_ptr<T> getComponent() {
        if (components.find(typeid(T).hash_code()) != components.end()) {
            return std::static_pointer_cast<T>(components[typeid(T).hash_code()]);
        }
        std::cout << this->tag << ": Component not found: " << typeid(T).name() << std::endl;
        return nullptr;
    }

    template <typename IT, typename T>
    std::shared_ptr<T> getComponent() {
        if (components.find(typeid(IT).hash_code()) != components.end()) {
            return std::static_pointer_cast<T>(components[typeid(IT).hash_code()]);
        }
        std::cout << this->tag << " : Component not found: " << typeid(IT).name() << std::endl;
        return nullptr;
    }

    template <typename T>
    bool removeComponent() {
        if (const size_t componentId = typeid(T).hash_code();
            components.find(componentId) != components.end()) {
            components.erase(componentId);
            for (int i = 0; i < components_vector.size(); i++) {
                if (components_vector[i] == componentId) {
                    components_vector.erase(components_vector.begin() + i);
                    break;
                }
            }
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
        for (const auto key : components_vector) {
            auto it = components.find(key);
            if (it == components.end()) {
                std::cerr << "Component not found: " << key << std::endl;
                continue;
            }
            if (components[key]->getActive())
                components[key]->update(deltaTime);
        }
    }

    void renderComponents(sf::RenderWindow* window) {
        for (const auto key : components_vector) {
            auto it = components.find(key);
            if (it == components.end()) {
                std::cerr << "Component not found: " << key << std::endl;
                continue;
            }
            if (components[key]->getActive())
                components[key]->render(window);
        }
    }

    void handleComponents(sf::Event& e) {
        for (const auto key : components_vector) {
            auto it = components.find(key);
            if (it == components.end()) {
                std::cerr << "Component not found: " << key << std::endl;
                continue;
            }
            if (components[key]->getActive())
                components[key]->handleEvent(e);
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
    std::vector<size_t> components_vector;
    inline static int idCounter = 0;
};


