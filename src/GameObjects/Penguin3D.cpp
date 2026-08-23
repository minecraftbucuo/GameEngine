//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD

#include "Penguin3D.h"
#include "ConfigManager.h"

Penguin3D::Penguin3D() {
    Model* p = ModelManager::getInstance().getModel("penguin");
    if (p == nullptr) {
        ModelManager::getInstance().loadModel(CONFIG.getModelPath("penguin"), "penguin");
        this->model = ModelManager::getInstance().getModel("penguin");
    } else {
        this->model = p;
    }
    position = {0.0f, -0.5f, 1.0f};
    className = "Penguin3D";
}

void Penguin3D::render(eng::Renderer& renderer) {
    drawFaces(renderer);
}

void Penguin3D::update(const eng::Time deltaTime) {
    angleXZ += deltaTime.asSeconds();
}
#endif
