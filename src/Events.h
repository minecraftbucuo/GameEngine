//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <memory>

struct CollisionEvent {
    std::shared_ptr<GameObject> a;
    std::shared_ptr<GameObject> b;
    sf::Vector2f a_speed;
    sf::Vector2f b_speed;
    sf::Vector2f a_position;
    sf::Vector2f b_position;
};


