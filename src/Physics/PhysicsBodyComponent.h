//
// Created by MINEC on 2026/8/18.
//

#pragma once

#include "Component.h"
#include "PhysicsTypes.h"
#include <box2d/box2d.h>
#include <SFML/System/Vector2.hpp>
#include "Core/Types.h"

class PhysicsBodyComponent : public Component {
public:
    PhysicsBodyComponent();
    ~PhysicsBodyComponent() override;

    PhysicsBodyComponent(const PhysicsBodyComponent&) = delete;
    PhysicsBodyComponent& operator=(const PhysicsBodyComponent&) = delete;

    void start() override;
    void update(const eng::Time& deltaTime) override;

    // body 配置（start() 前设置）
    void setBodyType(physics::BodyType type);
    void setDensity(float density);
    void setFriction(float friction);
    void setRestitution(float restitution);
    // 是否用矩形 fixture（默认根据 owner->size），否则用圆形
    void setShapeBox(float width, float height);
    void setShapeCircle(float radius);
    // 固定旋转（如角色不旋转）
    void setFixedRotation(bool fixed);
    // 线性/角阻尼（空气阻力，抑制抖动）
    void setLinearDamping(float damping);
    void setAngularDamping(float damping);
    void setCollisionFilter(uint16 categoryBits, uint16 maskBits);
    // 用 groupIndex：同正数始终碰撞，同负数始终不碰撞
    void setCollisionGroup(int16 groupIndex);

    // 施加力/冲量（像素单位，内部转米）
    void applyLinearImpulse(const eng::Vec2f& impulse);
    void applyForceToCenter(const eng::Vec2f& force);
    void setLinearVelocity(const eng::Vec2f& velocity);
    eng::Vec2f getLinearVelocity() const;

    // 直接设置位置/角度（用于网络同步或 kinematic）
    void setTransform(const eng::Vec2f& position, float angle = 0.0f);
    // 设置初始角度（start 前调用，start 时应用）
    void setInitialAngle(float angle);

    b2Body* getBody();
    const b2Body* getBody() const;

private:
    enum class ShapeType { Box, Circle };
    ShapeType shapeType = ShapeType::Box;
    float boxWidth = 0.0f;
    float boxHeight = 0.0f;
    float circleRadius = 0.0f;

    b2Body* body = nullptr;
    b2BodyType bodyType = b2_dynamicBody;
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
    bool fixedRotation = false;
    float initialAngle = 0.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    // 碰撞过滤（默认全通过）
    uint16 categoryBits = 0x0001;
    uint16 maskBits = 0xFFFF;
    int16 groupIndex = 0;
};
