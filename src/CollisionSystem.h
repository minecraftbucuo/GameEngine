//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
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

    // 空间查询（B4 broad-phase）：返回 AABB [min, max] 覆盖的 cell 内的候选对象。
    // 用于 needGravity 探针等"找某区域附近对象"的查询，替代全量遍历。
    // 候选可能重复（跨 cell），调用方需自行去重/幂等处理。
    [[nodiscard]] std::vector<std::shared_ptr<GameObject>> queryAABB(
        const sf::Vector2f& min, const sf::Vector2f& max) const;

private:
    // 迭代求解：速度多轮收敛（解决快照过期导致的堆叠振荡），位置一次全量修正
    void solveContacts(std::vector<Contact>& contacts);

    // 每帧重建空间哈希网格（O(n)），供碰撞检测与 queryAABB 使用
    void buildBroadPhase();

    // 生成一对候选对象（本 cell 与 3x3 邻域），用 GameObject id 去重避免重复对
    void collectPairs(std::vector<Contact>& contacts);

    [[nodiscard]] static std::pair<int, int> cellOf(const sf::Vector2f& pos);

    std::vector<std::shared_ptr<GameObject>> objects;

    // 空间哈希网格：cell 坐标 -> 该 cell 内的对象
    // 对象按其碰撞体 AABB 覆盖的**所有** cell 登记（大对象登记多个 cell），
    // 因此任意可能碰撞的对象对必然共享至少一个 cell（无 3x3 邻域漏检）。
    // CELL_SIZE 只影响性能平衡（小 = 更精细、大对象登记更多 cell）。
    static constexpr float CELL_SIZE = 128.f;
    std::unordered_map<uint64_t, std::vector<std::shared_ptr<GameObject>>> grid;
};
