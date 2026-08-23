//
// Created by MINEC on 2026/6/2.
//

#include "GravityComponent.h"
#include "MoveComponent.h"
#include "GameObject.h"
#include "Scene.h"
#include "Core/Types.h"

void GravityComponent::update(const eng::Time& deltaTime) {
    float worldHeight = owner->getScene()->getWindowSize().y;

    if (std::abs(this->owner->getPosition().y + this->owner->getSize().y - worldHeight) < 0.1f
        && std::abs(owner->getSpeed().y) <= 1.f) return;

    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) return;
    moveComponent->setSpeedY(owner->getSpeed().y + gravity * deltaTime.asSeconds());
}

std::string GravityComponent::getName() {
    return name;
}
