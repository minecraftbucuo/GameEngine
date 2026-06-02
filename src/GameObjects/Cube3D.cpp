//
// Created by MINEC on 2026/6/2.
//
#ifndef SERVER_BUILD
#include "Cube3D.h"
#include "ConfigManager.h"

Cube3D::Cube3D() {
    Model* p = ModelManager::getInstance().getModel("cube");
    if (p == nullptr) {
        ModelManager::getInstance().loadModel(CONFIG.getModelPath("cube"), "cube");
        this->model = ModelManager::getInstance().getModel("cube");
    } else {
        this->model = p;
    }
    position = {0.0f, 0.0f, 3.0f};
    className = "Cube3D";
}
#endif
