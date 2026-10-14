//
// Created by MINEC on 2025/12/10.
//


#pragma once
#include "Collision.h"
#include "CircleCollision.h"
#include "Core/Types.h"

class BoxCollision : public Collision {
public:
    BoxCollision(float x, float y, float width, float height);
    BoxCollision();
    void start() override;
    void update(const eng::Time& deltaTime) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

    [[nodiscard]] bool checkCollision(const Collision& other) const override;

    [[nodiscard]] bool checkCollisionWithBox(const BoxCollision& other) const override;

    [[nodiscard]] bool checkCollisionWithCircle(const CircleCollision &other) const override;


    [[nodiscard]] float getWidth() const;

    [[nodiscard]] float getHeight() const;

    [[nodiscard]] float getPosX() const;

    [[nodiscard]] float getPosY() const;

    // 基类 Collision::setPosition(const eng::Vec2f&) 是虚函数，本类的
    // setPosition(float, float) 是另一个重载。C++ 的名字隐藏规则下，派生类的
    // 同名函数会把基类那一组**全部遮蔽**：box.setPosition(vec2f) 直接编译失败，
    // 经基类指针调用则静默走基类实现。用 using 把基类重载引入本作用域，两者
    // 共存（参数个数不同，调用无歧义）。GCC 对此报 -Woverloaded-virtual。
    using Collision::setPosition;
    void setPosition(float x, float y);

    void setSize(float width_, float height_);

private:
    eng::Vec2f size;
};





