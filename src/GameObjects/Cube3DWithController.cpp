//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Cube3DWithController.h"
#include "Core/Input.h"
#include "Scene.h"

Cube3DWithController::Cube3DWithController() {
    className = "Cube3DWithController";
}

void Cube3DWithController::start() {
    GameObject::start();
    if (auto* cam = getScene()->getCamera()) {
        cam->setMouseControl(false);
    }
}

void Cube3DWithController::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::MouseButtonPress) {
        mouse_is_pressed = true;
        mousePos = eng::Input::getMousePosition();
    } else if (event.type == eng::EventType::MouseButtonRelease) {
        mouse_is_pressed = false;
        for (auto& point : model->points) {
            point = rotate(point);
        }
        angleXZ = 0.0f;
        angleYZ = 0.0f;
    } else if (event.type == eng::EventType::MouseMove) {
        const eng::Vec2i pos = eng::Input::getMousePosition();
        if (mouse_is_pressed) {
            const eng::Vec2i delta = pos - mousePos;
            angleYZ -= delta.y * 0.01f;
            angleXZ += delta.x * 0.01f;
            mousePos = pos;
        }
    }
}
#endif