//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "MarioCameraComponent.h"
#include "Camera.h"
#include "GameObject.h"
#include "Scene.h"

void MarioCameraComponent::start() {
    if (const Camera* camera = owner->getScene()->getCamera()) {
        this->position = camera->getPosition();
    }
}

void MarioCameraComponent::update(const eng::Time& deltaTime) {
    if (owner->getPosition().x > 500) this->setTargetPositionX(owner->getPosition().x - 500);
    else this->setTargetPositionX(0);
    if (this->target_position != this->position) {
        if (Camera* camera = owner->getScene()->getCamera()) {
            auto add_position = (target_position - position) * 0.03f;
            if (std::abs(add_position.x) < 0.2f) add_position.x = 0.f;
            position = position + add_position;
            camera->setPosition(position.x, position.y);
        }
    }
}

void MarioCameraComponent::setTargetPosition(const eng::Vec2f& pos) {
    this->target_position = pos;
}

void MarioCameraComponent::setTargetPositionX(const float x) {
    this->target_position.x = x;
}

#endif
