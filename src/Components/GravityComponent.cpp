//
// Created by MINEC on 2026/6/2.
//

#include "GravityComponent.h"
#include "MoveComponent.h"
#include "GameObject.h"
#include "Scene.h"
#include "CollisionSystem.h"
#include "Core/Types.h"

void GravityComponent::update(const eng::Time& deltaTime) {
    auto* scene = owner->getScene();
    if (!scene) return;

    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) return;

    if (smart) {
        if (auto* collisionSystem = scene->getCollisionSystem();
            collisionSystem && collisionSystem->isStanding(owner)) {
            return;
        }
    }
    else {
        const float worldHeight = scene->getWindowSize().y;
        if (std::abs(owner->getPosition().y + owner->getSize().y - worldHeight) < 0.1f
            && std::abs(owner->getSpeed().y) <= 1.f) return;
    }

    moveComponent->setSpeedY(owner->getSpeed().y + gravity * deltaTime.asSeconds());
}

std::string GravityComponent::getName() {
    return name;
}
