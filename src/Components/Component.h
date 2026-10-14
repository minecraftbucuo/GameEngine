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

    // 以下虚函数的基类默认实现为空：参数只用于对齐派生类签名，故省略形参名
    // （-Wunused-parameter 的惯用消法；派生类覆写时按需自行命名）。
    virtual void update(const eng::Time&) {
    }

    // 渲染签名（SDL3 迁移 6e：旧 sf::RenderWindow 签名已删除，唯一虚签名）
    virtual void render(eng::Renderer&) {
    }

    virtual void handleEvent(const eng::EngineEvent&) {
    }

    void setOwner(GameObject* obj);
    [[nodiscard]] GameObject* getOwner() const;
    void setActive(bool value);
    [[nodiscard]] bool getActive() const;

protected:
    GameObject* owner{};
    bool active = true;
};

