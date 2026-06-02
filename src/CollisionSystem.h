//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <vector>
#include <memory>

class GameObject;

class CollisionSystem {
public:
    void addObject(const std::shared_ptr<GameObject>& obj);
    void checkCollisions();

    [[nodiscard]] std::vector<std::shared_ptr<GameObject>>* getObjects();

private:
    std::vector<std::shared_ptr<GameObject>> objects;
};


