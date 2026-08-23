//
// Created by MINEC on 2026/1/4.
//

#pragma once
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Types.h"

struct Model {
    std::vector<eng::Vec3f> points;
    std::vector<std::vector<int>> faces;
};

class ModelManager {
public:
    static ModelManager& getInstance() {
        static ModelManager instance;
        return instance;
    }

    void loadModel(const std::string& file_name, std::string name = "default");

    Model* getModel(const std::string& name) {
        if (models.find(name) == models.end()) return nullptr;
        return &models[name];
    }

private:
    ModelManager() = default;
    int count = 0;
    std::unordered_map<std::string, Model> models{};
};