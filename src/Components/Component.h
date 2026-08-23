//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "Core/Types.h"
#include "Core/Event.h"
#include "Render/Renderer.h"

class GameObject;

class Component {
public:
    Component() = default;

    explicit Component(GameObject* owner) : owner(owner) {
    }

    virtual ~Component() = default;

    virtual void start() {
    }

    virtual void update(const eng::Time& deltaTime) {
    }

    // 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除，唯一虚签名）
    virtual void render(eng::Renderer& renderer) {
        (void)renderer;
    }

    virtual void handleEvent(const eng::EngineEvent& event) {
    }

    void setOwner(GameObject* obj);
    [[nodiscard]] GameObject* getOwner() const;
    void setActive(bool value);
    [[nodiscard]] bool getActive() const;

protected:
    GameObject* owner{};
    bool active = true;
};

