//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "Component.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <typeindex>
#include "Logger.h"

class Scene;

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

    virtual void start();

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
        const std::type_index componentId(typeid(T));
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        return component;
    }

    template <typename IT, typename T, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args) {
        const std::type_index componentId(typeid(IT));
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        components[componentId] = component;
        components_vector.emplace_back(componentId);
        component->setOwner(this);
        return component;
    }

    template <typename T>
    std::shared_ptr<T> getComponent() {
        const std::type_index key(typeid(T));
        if (components.find(key) != components.end()) {
            return std::static_pointer_cast<T>(components[key]);
        }
        LOG_INFO_FMT("{} : Component not found: {}", this->tag, typeid(T).name());
        return nullptr;
    }

    template <typename IT, typename T>
    std::shared_ptr<T> getComponent() {
        const std::type_index key(typeid(IT));
        if (components.contains(key)) {
            return std::static_pointer_cast<T>(components[key]);
        }
        LOG_INFO_FMT("{} : Component not found: {}", this->tag, typeid(IT).name());
        return nullptr;
    }

    template <typename T>
    bool removeComponent() {
        if (const std::type_index componentId(typeid(T));
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

    // 逆质量（invMass）：用于碰撞冲量按质量分配。
    // 0 = 无穷质量（静态/不参与求解器物理，碰撞冲量与位置修正全部分给对方）；
    // 默认 1 = 单位质量。
    // 注意：moveAble=false 的对象在求解器中一律按 invMass=0 处理；
    // 需要"自己不被求解器动、但仍推动对方"的对象（如 Mario/Box/FireBall）在构造时 setInvMass(0)。
    float getInvMass() const {
        return invMass;
    }

    void setInvMass(const float mass) {
        invMass = mass;
    }

    Scene* getScene() const {
        return scene;
    }

    void setScene(Scene* s) {
        scene = s;
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
    float invMass{1.f};
    bool started;
    bool is_destroy{false};
    unsigned int id;
    std::string tag = "game_object:";
    std::string className = "GameObject";
    Scene* scene{};
    std::unordered_map<std::type_index, std::shared_ptr<Component>> components;
    std::vector<std::type_index> components_vector;
    inline static unsigned int idCounter = 0;
};


