//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "Component.h"
#include <unordered_map>
#include <memory>
#include <string>
#include "Logger.h"

class GameObject {
    friend class MoveComponent;
    friend class StateMachine;

public:
    GameObject();
    GameObject(float posX, float posY, float width, float height);
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

    bool isDestroy() const {
        return is_destroy;
    }

    virtual void destroy() {
        is_destroy = true;
        active = false;
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
        LOG_INFO_FMT("{} : Component not found: {}", this->tag, typeid(T).name());
        return nullptr;
    }

    template <typename IT, typename T>
    std::shared_ptr<T> getComponent() {
        if (components.contains(typeid(IT).hash_code())) {
            return std::static_pointer_cast<T>(components[typeid(IT).hash_code()]);
        }
        LOG_INFO_FMT("{} : Component not found: {}", this->tag, typeid(IT).name());
        return nullptr;
    }

    template <typename T>
    bool removeComponent() {
        if (const size_t componentId = typeid(T).hash_code();
            components.contains(componentId)) {
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

    virtual sf::Vector2f getCenter() {
        return position + size / 2.f;
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

    unsigned int getId() const {
        return id;
    }

    void setId(const unsigned int _id) {
        this->id = _id;
    }

    const std::string& getClassName() const {
        return className;
    }

    void updateComponents(sf::Time deltaTime);

    void renderComponents(sf::RenderWindow* window);

    void handleComponents(sf::Event& e);

    static void resetIdCounter() {
        idCounter = 0;
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
    bool is_destroy{false};
    unsigned int id;
    std::string tag = "game_object:";
    std::string className = "GameObject";
    std::unordered_map<size_t, std::shared_ptr<Component>> components;
    std::vector<size_t> components_vector;
    inline static unsigned int idCounter = 0;
};


