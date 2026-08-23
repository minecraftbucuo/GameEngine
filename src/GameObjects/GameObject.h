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
#include "Core/Types.h"
#include "Core/Event.h"

class Scene;
class PhysicsBodyComponent;

class GameObject {
    friend class MoveComponent;
    friend class StateMachine;
    friend class PhysicsBodyComponent;

public:
    GameObject();
    GameObject(float posX, float posY, float width, float height);
    virtual ~GameObject() = default;

    virtual void handleEvent(const eng::EngineEvent& e) {
        handleComponents(e);
    }

    virtual void update(eng::Time deltaTime) {
        updateComponents(deltaTime);
    }

    // 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除；
    // 基类默认只跑组件循环，子类按需 override）
    virtual void render(eng::Renderer& renderer) {
        renderComponents(renderer);
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

    const eng::Vec2f& getPosition() const {
        return position;
    }

    virtual eng::Vec2f getCenter() {
        return position + size / 2.f;
    }

    const eng::Vec2f& getSize() const {
        return size;
    }

    const eng::Vec2f& getSpeed() const {
        return speed;
    }

    const std::string& getTag() const {
        return tag;
    }

    bool getMoveAble() const {
        return moveAble;
    }

    void setSize(float width, float height) {
        size = eng::Vec2f(width, height);
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

    Scene* getScene() const {
        return scene;
    }

    void setScene(Scene* s) {
        scene = s;
    }

    void updateComponents(eng::Time deltaTime);

    void renderComponents(eng::Renderer& renderer);

    void handleComponents(const eng::EngineEvent& e);

    static void resetIdCounter() {
        idCounter = 0;
    }

protected:
    virtual void setPosition(const float posX, const float posY) {
        position = eng::Vec2f(posX, posY);
    }

    eng::Vec2f position;
    eng::Vec2f size;
    eng::Vec2f speed;
    float rotation = 0.0f; // 度数，用于物理体旋转同步
    bool active;
    bool moveAble{true};
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


