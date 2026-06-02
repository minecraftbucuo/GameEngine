//
// Created by MINEC on 2026/6/2.
//
#ifndef SERVER_BUILD

#include "Human3D.h"
#include "ConfigManager.h"

Human3D::Human3D() {
    Model* p = ModelManager::getInstance().getModel("human");
    if (p == nullptr) {
        ModelManager::getInstance().loadModel(CONFIG.getModelPath("human"), "human");
        this->model = ModelManager::getInstance().getModel("human");
    } else {
        this->model = p;
    }
    position = {0.0f, -3.0f, 5.0f};
    className = "Human3D";
}

void Human3D::render(sf::RenderWindow* window) {
    drawFaces(window);
}

void Human3D::update(const sf::Time deltaTime) {
    angleXZ += deltaTime.asSeconds();
}
#endif
