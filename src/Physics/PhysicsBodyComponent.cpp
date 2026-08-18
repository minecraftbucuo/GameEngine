//
// Created by MINEC on 2026/8/18.
//

#include "PhysicsBodyComponent.h"
#include "PhysicsWorld.h"
#include "GameObject.h"
#include "Scene.h"
#include "Logger.h"

PhysicsBodyComponent::PhysicsBodyComponent() = default;

PhysicsBodyComponent::~PhysicsBodyComponent() {
    // 析构时销毁 body，避免悬挂指针
    if (body) {
        if (auto* scene = owner ? owner->getScene() : nullptr) {
            if (auto* world = scene->getPhysicsWorld()) {
                world->destroyBody(body);
            }
        }
        body = nullptr;
    }
}

void PhysicsBodyComponent::start() {
    if (body) return; // 已创建
    auto* scene = owner ? owner->getScene() : nullptr;
    if (!scene) {
        LOG_ERROR("PhysicsBodyComponent::start() - owner has no scene");
        return;
    }
    auto* world = scene->getPhysicsWorld();
    if (!world) {
        LOG_ERROR("PhysicsBodyComponent::start() - scene has no PhysicsWorld");
        return;
    }

    // body 定义
    b2BodyDef def;
    def.type = bodyType;
    def.position = physics::toMeters(owner->getPosition());
    def.fixedRotation = fixedRotation;
    body = world->createBody(&def);

    // fixture 形状
    if (shapeType == ShapeType::Box) {
        // 未显式设置尺寸时用 owner 的 size
        float w = boxWidth > 0 ? boxWidth : owner->getSize().x;
        float h = boxHeight > 0 ? boxHeight : owner->getSize().y;
        b2PolygonShape shape;
        shape.SetAsBox(physics::toMeters(w * 0.5f), physics::toMeters(h * 0.5f));
        b2FixtureDef fd;
        fd.shape = &shape;
        fd.density = density;
        fd.friction = friction;
        fd.restitution = restitution;
        body->CreateFixture(&fd);
    } else {
        // 圆形
        float r = circleRadius > 0 ? circleRadius : owner->getSize().x * 0.5f;
        b2CircleShape shape;
        shape.m_radius = physics::toMeters(r);
        b2FixtureDef fd;
        fd.shape = &shape;
        fd.density = density;
        fd.friction = friction;
        fd.restitution = restitution;
        body->CreateFixture(&fd);
    }
}

void PhysicsBodyComponent::update(const sf::Time& deltaTime) {
    if (!body || !owner) return;
    // 把 b2Body 位置（米）回写到 owner->position（像素）
    b2Vec2 pos = body->GetPosition();
    owner->position = physics::toPixels(pos);
}

void PhysicsBodyComponent::setBodyType(physics::BodyType type) {
    bodyType = static_cast<b2BodyType>(type);
}

void PhysicsBodyComponent::setDensity(float d) {
    density = d;
}

void PhysicsBodyComponent::setFriction(float f) {
    friction = f;
}

void PhysicsBodyComponent::setRestitution(float r) {
    restitution = r;
}

void PhysicsBodyComponent::setShapeBox(float width, float height) {
    shapeType = ShapeType::Box;
    boxWidth = width;
    boxHeight = height;
}

void PhysicsBodyComponent::setShapeCircle(float radius) {
    shapeType = ShapeType::Circle;
    circleRadius = radius;
}

void PhysicsBodyComponent::setFixedRotation(bool fixed) {
    fixedRotation = fixed;
}

void PhysicsBodyComponent::applyLinearImpulse(const sf::Vector2f& impulse) {
    if (body) {
        body->ApplyLinearImpulseToCenter(physics::toMeters(impulse), true);
    }
}

void PhysicsBodyComponent::applyForceToCenter(const sf::Vector2f& force) {
    if (body) {
        body->ApplyForceToCenter(physics::toMeters(force), true);
    }
}

void PhysicsBodyComponent::setLinearVelocity(const sf::Vector2f& velocity) {
    if (body) {
        body->SetLinearVelocity(physics::toMeters(velocity));
    }
}

sf::Vector2f PhysicsBodyComponent::getLinearVelocity() const {
    if (!body) return {};
    return physics::toPixels(body->GetLinearVelocity());
}

void PhysicsBodyComponent::setTransform(const sf::Vector2f& position, float angle) {
    if (body) {
        body->SetTransform(physics::toMeters(position), angle);
    }
}

b2Body* PhysicsBodyComponent::getBody() {
    return body;
}

const b2Body* PhysicsBodyComponent::getBody() const {
    return body;
}
