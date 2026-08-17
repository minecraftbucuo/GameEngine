//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <vector>
#include <memory>
#include <SFML/System/Vector2.hpp>

class GameObject;

// 一次碰撞接触：几何信息（normal/penetration）+ 事件快照（检测帧开始时的状态，用于发布碰撞事件）
struct Contact {
    std::shared_ptr<GameObject> a;
    std::shared_ptr<GameObject> b;
    sf::Vector2f normal;      // 从 b 指向 a 的单位法向
    float penetration = 0.f;  // 穿透深度
    sf::Vector2f a_speed;
    sf::Vector2f b_speed;
    sf::Vector2f a_position;
    sf::Vector2f b_position;
};

class CollisionSystem {
public:
    void addObject(const std::shared_ptr<GameObject>& obj);
    void checkCollisions();

    [[nodiscard]] std::vector<std::shared_ptr<GameObject>>* getObjects();

private:
    // 迭代求解：速度多轮收敛（解决快照过期导致的堆叠振荡），位置一次全量修正
    void solveContacts(std::vector<Contact>& contacts);

    std::vector<std::shared_ptr<GameObject>> objects;
};
