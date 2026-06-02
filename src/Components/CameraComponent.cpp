//
// Created by MINEC on 2026/6/2.
//

#ifndef SERVER_BUILD
#include "CameraComponent.h"

#include "Camera.h"
#include "SceneContext.h"
#include "GameObject.h"

void CameraComponent::update(const sf::Time& deltaTime) {
    if (Camera* camera = SceneContext::getInstance().getCamera()) {
        camera->setPosition(owner->getPosition().x - 400, owner->getPosition().y - 600);
    }
}
#endif