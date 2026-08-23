//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "NewModel3D.h"
#include "ConfigManager.h"

NewModel3D::NewModel3D() {
    Model* p = ModelManager::getInstance().getModel("new_model");
    if (p == nullptr) {
        ModelManager::getInstance().loadModel(CONFIG.getModelPath("newModel"), "new_model");
        this->model = ModelManager::getInstance().getModel("new_model");
    }
    else {
        this->model = p;
    }
    position = {0.f, -0.6f, 1.f};
    className = "NewModel3D";
}

void NewModel3D::render(sf::RenderWindow* window) {
    drawFaces(window);
}

void NewModel3D::update(const eng::Time deltaTime) {
    angleXZ += deltaTime.asSeconds();
}
#endif
