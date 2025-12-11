//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <memory>

struct CollisionEvent {
    std::shared_ptr<GameObject> a;
    std::shared_ptr<GameObject> b;
};


