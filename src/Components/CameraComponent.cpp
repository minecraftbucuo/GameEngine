//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "CameraComponent.h"

#include "Camera.h"
#include "GameObject.h"
#include "Scene.h"

void CameraComponent::update(const eng::Time& deltaTime) {
    if (Camera* camera = owner->getScene()->getCamera()) {
        camera->setPosition(owner->getPosition().x - 400, owner->getPosition().y - 600);
    }
}
#endif