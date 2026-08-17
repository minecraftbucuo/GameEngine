//
// Created by MINEC on 2026/6/2.
//

#include "CollisionSystem.h"

#include "Collision.h"
#include "CircleCollision.h"
#include "BoxCollision.h"
#include "GameObject.h"
#include "MoveComponent.h"
#include "EventBus.h"
#include "Events.h"
#include "ConfigManager.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

// 几何判定阈值（数值稳定性，无需配置）
constexpr float EPSILON = 1e-6f;

// ---------- 几何：计算法向与穿透深度（normal 统一为"从 b 指向 a"） ----------

void computeCircleCircle(const CircleCollision* a, const CircleCollision* b, Contact& c) {
    const sf::Vector2f d = a->getCenter() - b->getCenter();
    const float dist = std::sqrt(d.x * d.x + d.y * d.y);
    if (dist > EPSILON) {
        c.normal = d / dist;
    } else {
        // 圆心精确重合：取安全方向（水平优先，完全重合向上），避免除零 NaN
        if (std::abs(d.x) > EPSILON) {
            c.normal = sf::Vector2f(d.x > 0.f ? 1.f : -1.f, 0.f);
        } else if (std::abs(d.y) > EPSILON) {
            c.normal = sf::Vector2f(0.f, d.y > 0.f ? 1.f : -1.f);
        } else {
            c.normal = sf::Vector2f(0.f, -1.f);
        }
    }
    c.penetration = a->getRadius() + b->getRadius() - dist;
}

// circle 在 a、box 在 b：normal 从 b(box) 指向 a(circle)
void computeCircleBox(const CircleCollision* circle, const BoxCollision* box, Contact& c) {
    const sf::Vector2f center = circle->getCenter();
    const sf::Vector2f min = box->getCollisionPosition();
    const sf::Vector2f max = min + sf::Vector2f(box->getWidth(), box->getHeight());
    const sf::Vector2f closest(std::max(min.x, std::min(center.x, max.x)),
                               std::max(min.y, std::min(center.y, max.y)));
    const sf::Vector2f delta = center - closest;
    const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const float radius = circle->getRadius();
    if (dist > EPSILON) {
        c.normal = delta / dist;
        c.penetration = radius - dist;
    } else {
        // 圆心在 box 内部：取最近边作为分离方向
        const float left = center.x - min.x, right = max.x - center.x;
        const float top = center.y - min.y, bottom = max.y - center.y;
        if (left <= right && left <= top && left <= bottom) {
            c.normal = sf::Vector2f(-1.f, 0.f);
            c.penetration = radius + left;
        } else if (right <= top && right <= bottom) {
            c.normal = sf::Vector2f(1.f, 0.f);
            c.penetration = radius + right;
        } else if (top <= bottom) {
            c.normal = sf::Vector2f(0.f, -1.f);
            c.penetration = radius + top;
        } else {
            c.normal = sf::Vector2f(0.f, 1.f);
            c.penetration = radius + bottom;
        }
    }
}

// 均为 box：normal 从 b 指向 a（最小重叠轴）
void computeBoxBox(const BoxCollision* a, const BoxCollision* b, Contact& c) {
    const sf::Vector2f min_a = a->getCollisionPosition();
    const sf::Vector2f max_a = min_a + sf::Vector2f(a->getWidth(), a->getHeight());
    const sf::Vector2f min_b = b->getCollisionPosition();
    const sf::Vector2f max_b = min_b + sf::Vector2f(b->getWidth(), b->getHeight());
    const float overlap_x = std::min(max_a.x, max_b.x) - std::max(min_a.x, min_b.x);
    const float overlap_y = std::min(max_a.y, max_b.y) - std::max(min_a.y, min_b.y);
    const sf::Vector2f center_a = a->getCenter();
    const sf::Vector2f center_b = b->getCenter();
    if (overlap_x < overlap_y) {
        c.normal = center_a.x < center_b.x ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
        c.penetration = overlap_x;
    } else {
        c.normal = center_a.y < center_b.y ? sf::Vector2f(0.f, -1.f) : sf::Vector2f(0.f, 1.f);
        c.penetration = overlap_y;
    }
}

void computeContact(Contact& c, const Collision* a_c, const Collision* b_c) {
    const auto* a_circle = dynamic_cast<const CircleCollision*>(a_c);
    const auto* b_circle = dynamic_cast<const CircleCollision*>(b_c);
    const auto* a_box = dynamic_cast<const BoxCollision*>(a_c);
    const auto* b_box = dynamic_cast<const BoxCollision*>(b_c);
    if (a_circle && b_circle) {
        computeCircleCircle(a_circle, b_circle, c);
    } else if (a_circle && b_box) {
        computeCircleBox(a_circle, b_box, c);
    } else if (a_box && b_circle) {
        computeCircleBox(b_circle, a_box, c);
        c.normal = -c.normal;  // 翻转：normal 需统一为"从 b 指向 a"
    } else if (a_box && b_box) {
        computeBoxBox(a_box, b_box, c);
    }
}

// ---------- 求解辅助 ----------

// 有效逆质量：静态对象（moveAble=false）与 invMass=0 的对象（如 Mario/Box/FireBall，
// 各自在构造时 setInvMass(0) 声明"不参与求解器物理"）不参与动量交换，
// 冲量/位置修正全部分给对方（调用处已乘 invMass，invMass=0 时自动为零）。
float invMassOf(const std::shared_ptr<GameObject>& obj) {
    if (!obj->getMoveAble()) return 0.f;
    return obj->getInvMass();
}

void applyImpulse(const std::shared_ptr<GameObject>& obj, const sf::Vector2f& dv) {
    if (!obj->getMoveAble()) return;
    if (const auto mc = obj->getComponent<MoveComponent>(); mc && mc->getActive()) {
        mc->addSpeed(dv);
    }
}

void applyCorrection(const std::shared_ptr<GameObject>& obj, const sf::Vector2f& dp) {
    if (!obj->getMoveAble()) return;
    if (const auto mc = obj->getComponent<MoveComponent>(); mc && mc->getActive()) {
        mc->addPosition(dp);
    }
}

}  // namespace

void CollisionSystem::addObject(const std::shared_ptr<GameObject>& obj) {
    objects.push_back(obj);
}

void CollisionSystem::checkCollisions() {

    // 清理已经销毁的游戏对象
    std::erase_if(objects, [](const auto& obj) {
        return obj->isDestroy();
    });

    // 重建空间哈希网格（B4 broad-phase，O(n)）
    buildBroadPhase();

    // 第一阶段：检测 → Contact 列表（几何基于帧开始时的快照，与事件快照一致）
    std::vector<Contact> contacts;
    collectPairs(contacts);

    // 第二阶段：迭代求解物理响应
    solveContacts(contacts);

    // 第三阶段：发布碰撞事件（仅用于游戏逻辑：伤害/状态/爆炸判定等；
    // 物理响应已由求解器统一处理，订阅者不应再修改位置/速度）
    for (const auto& c : contacts) {
        EventBus::getInstance().publish("onCollision" + c.a->getTag(),
            CollisionEvent{ c.a, c.b, c.a_speed, c.b_speed, c.a_position, c.b_position });
        EventBus::getInstance().publish("onCollision" + c.b->getTag(),
            CollisionEvent{ c.b, c.a, c.b_speed, c.a_speed, c.b_position, c.a_position });
    }
}

std::pair<int, int> CollisionSystem::cellOf(const sf::Vector2f& pos) {
    return {static_cast<int>(std::floor(pos.x / CELL_SIZE)),
            static_cast<int>(std::floor(pos.y / CELL_SIZE))};
}

namespace {
uint64_t makeCellKey(const int cx, const int cy) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
           static_cast<uint32_t>(cy);
}
}  // namespace

void CollisionSystem::buildBroadPhase() {
    grid.clear();
    for (const auto& obj : objects) {
        if (!obj->isActive()) continue;
        const auto collision = obj->getComponent<Collision>();
        if (!collision || !collision->getActive()) continue;
        // 对象登记到其碰撞体 AABB 覆盖的**所有** cell：
        // 大对象（地面/墙等超大 AABB）因此能与远处小对象共享 cell，
        // 避免"只登记中心 cell + 3x3 邻域"导致的大对象漏检（穿墙）。
        const sf::Vector2f min = collision->getCollisionPosition();
        const sf::Vector2f max = min + collision->getSize();
        const auto [x0, y0] = cellOf(min);
        const auto [x1, y1] = cellOf(max);
        for (int cx = x0; cx <= x1; cx++) {
            for (int cy = y0; cy <= y1; cy++) {
                grid[makeCellKey(cx, cy)].push_back(obj);
            }
        }
    }
}

void CollisionSystem::collectPairs(std::vector<Contact>& contacts) {
    // 一对候选对象的检测（原全量遍历的检测逻辑，抽取为 lambda）
    auto tryPair = [&contacts](const std::shared_ptr<GameObject>& a,
                               const std::shared_ptr<GameObject>& b) {
        if (!a->getMoveAble() && !b->getMoveAble()) return;
        if (!a->isActive() || !b->isActive()) return;

        const auto a_c = a->getComponent<Collision>();
        if (!a_c || !a_c->getActive()) return;

        if (const auto b_c = b->getComponent<Collision>();
            b_c && b_c->getActive() && a_c->checkCollision(*b_c)) {
            Contact contact;
            contact.a = a;
            contact.b = b;
            contact.a_speed = a->getSpeed();
            contact.b_speed = b->getSpeed();
            contact.a_position = a_c->getCollisionPosition();
            contact.b_position = b_c->getCollisionPosition();
            computeContact(contact, a_c.get(), b_c.get());
            contacts.push_back(contact);
        }
    };

    // 每个 cell 内两两配对。由于对象登记到 AABB 覆盖的所有 cell，
    // 任何可能碰撞的对象对必然共享至少一个 cell；同一对可能出现在多个
    // 共同 cell 中，用规范化 id 对去重，保证每对只检查一次。
    std::unordered_set<uint64_t> seen;
    for (const auto& [key, objs] : grid) {
        (void)key;
        for (size_t i = 0; i < objs.size(); i++) {
            for (size_t j = i + 1; j < objs.size(); j++) {
                const auto& a = objs[i];
                const auto& b = objs[j];
                const uint64_t pair_key = (a->getId() < b->getId())
                    ? (static_cast<uint64_t>(a->getId()) << 32 | b->getId())
                    : (static_cast<uint64_t>(b->getId()) << 32 | a->getId());
                if (!seen.insert(pair_key).second) continue;
                tryPair(a, b);
            }
        }
    }
}

std::vector<std::shared_ptr<GameObject>> CollisionSystem::queryAABB(
    const sf::Vector2f& min, const sf::Vector2f& max) const {
    std::vector<std::shared_ptr<GameObject>> result;
    const auto [x0, y0] = cellOf(min);
    const auto [x1, y1] = cellOf(max);
    for (int cx = x0; cx <= x1; cx++) {
        for (int cy = y0; cy <= y1; cy++) {
            const auto it = grid.find(makeCellKey(cx, cy));
            if (it == grid.end()) continue;
            result.insert(result.end(), it->second.begin(), it->second.end());
        }
    }
    return result;
}

void CollisionSystem::solveContacts(std::vector<Contact>& contacts) {
    // 速度迭代：多轮收敛，每轮使用最新速度，让相互耦合的接触（堆叠）逐步逼近一致解，
    // 消除"处理顺序依赖 + 快照过期"导致的振荡
    for (int iter = 0; iter < CONFIG.game.solverIterations; iter++) {
        for (auto& c : contacts) {
            const float inv_ma = invMassOf(c.a);
            const float inv_mb = invMassOf(c.b);
            const float inv_sum = inv_ma + inv_mb;
            if (inv_sum <= 0.f) continue;  // 双方都不可动（理论上已被检测过滤）

            const sf::Vector2f va = c.a->getSpeed();
            const sf::Vector2f vb = c.b->getSpeed();
            const float v_rel_n = (va.x - vb.x) * c.normal.x + (va.y - vb.y) * c.normal.y;
            if (v_rel_n >= 0.f) continue;  // 正在分离或相对静止，无需处理

            if (-v_rel_n < CONFIG.game.restingSpeedThreshold) {
                // 接触静止：施加 e=0 的冲量消除法向相对速度（完全非弹性），
                // 速度按逆质量分配，不再注入回弹（避免持续注能微弹跳）
                const float j = -v_rel_n / inv_sum;
                applyImpulse(c.a, c.normal * j * inv_ma);
                applyImpulse(c.b, -c.normal * j * inv_mb);
            } else {
                // 快速接近：回弹冲量（恢复系数模型）。
                // 冲量 = -(1+e) × v_rel_n / (invMa+invMb)，速度按逆质量分配，
                // 分离速度 = e × 接近速度（动量守恒）。
                // 双方都有质量（invMass > 0）才用"移动-移动"的小回弹；
                // 对方 invMass=0（静态/不参与求解器，如地面、Mario、Box、FireBall）视为撞墙。
                const bool both_mov = inv_ma > 0.f && inv_mb > 0.f;
                const float restitution = both_mov ? CONFIG.game.restitutionMoving
                                                   : CONFIG.game.restitutionStatic;
                const float j = -(1.f + restitution) * v_rel_n / inv_sum;
                applyImpulse(c.a, c.normal * j * inv_ma);
                applyImpulse(c.b, -c.normal * j * inv_mb);
            }
        }
    }

    // 位置修正：一次全量分摊（A4 决定全量最快收敛），按逆质量比例分配。
    // 在速度迭代之后做一次，避免多轮迭代累计 overshoot。
    for (auto& c : contacts) {
        const float inv_ma = invMassOf(c.a);
        const float inv_mb = invMassOf(c.b);
        const float inv_sum = inv_ma + inv_mb;
        if (inv_sum <= 0.f) continue;
        applyCorrection(c.a, c.normal * c.penetration * (inv_ma / inv_sum));
        applyCorrection(c.b, -c.normal * c.penetration * (inv_mb / inv_sum));
    }
}

std::vector<std::shared_ptr<GameObject>>* CollisionSystem::getObjects() {
    return &objects;
}
