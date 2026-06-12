//
// Created by MINEC on 2026/6/2.
//

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

void MarioCameraComponent::update(const sf::Time& deltaTime) {
    if (owner->getPosition().x > 500) this->setTargetPositionX(owner->getPosition().x - 500);
    else this->setTargetPositionX(0);
    if (this->target_position != this->position) {
        if (Camera* camera = owner->getScene()->getCamera()) {
            position = position + (target_position - position) * 0.03f;
            camera->setPosition(position.x, position.y);
        }
    }
}

void MarioCameraComponent::setTargetPosition(const sf::Vector2f& pos) {
    this->target_position = pos;
}

void MarioCameraComponent::setTargetPositionX(const float x) {
    this->target_position.x = x;
}

#endif
