//
// Created by MINEC on 2026/6/2.
//

#ifndef SERVER_BUILD
#include "Cube3DWithController.h"
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

void Cube3DWithController::handleEvent(sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed) {
        mouse_is_pressed = true;
        mousePos = sf::Mouse::getPosition(*getScene()->getWindow());
    } else if (event.type == sf::Event::MouseButtonReleased) {
        mouse_is_pressed = false;
        for (auto& point : model->points) {
            point = rotate(point);
        }
        angleXZ = 0.0f;
        angleYZ = 0.0f;
    } else if (event.type == sf::Event::MouseMoved) {
        const sf::Vector2i pos = sf::Mouse::getPosition(*getScene()->getWindow());
        if (mouse_is_pressed) {
            const sf::Vector2i delta = pos - mousePos;
            angleYZ -= delta.y * 0.01f;
            angleXZ += delta.x * 0.01f;
            mousePos = pos;
        }
    }
}
#endif