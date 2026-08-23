//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <memory>
#include "GameObject.h"
#include "Core/Types.h"

struct CollisionEvent {
    std::shared_ptr<GameObject> a;
    std::shared_ptr<GameObject> b;
    eng::Vec2f a_speed;
    eng::Vec2f b_speed;
    eng::Vec2f a_position;
    eng::Vec2f b_position;
};


