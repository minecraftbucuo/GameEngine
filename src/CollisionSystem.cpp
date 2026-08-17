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
#include <algorithm>
#include <cmath>

namespace {

// 迭代求解轮数：多轮迭代让相互耦合的接触（堆叠）逐步收敛，消除"处理顺序依赖"
constexpr int SOLVER_ITERATIONS = 4;
// 接触静止阈值：法向接近速度低于该值视为静止接触，清除法向速度、不再回弹
constexpr float RESTING_SPEED_THRESHOLD = 40.f;
// 恢复系数 e：与静态物体（地面/墙）碰撞保留弹跳手感（球落地）；
// 移动物体之间用小回弹抑制堆叠振荡（球叠球）。
// 冲量模型：分离速度 = e × 接近速度，冲量 = -(1+e) × v_rel_n
constexpr float BOUNCE_FACTOR_STATIC = 0.28f;
constexpr float BOUNCE_FACTOR_MOVING = 0.1f;
// 几何判定阈值
constexpr float EPSILON = 1e-6f;

// ---------- 几何：计算法向与穿透深度（normal 统一为"从 b 指向 a"） ----------

void computeCircleCircle(const CircleCollision* a, const CircleCollision* b, Contact& c) {
    const sf::Vector2f d = a->getPos() - b->getPos();
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
    const sf::Vector2f center = circle->getPos();
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
    const sf::Vector2f center_a = (min_a + max_a) * 0.5f;
    const sf::Vector2f center_b = (min_b + max_b) * 0.5f;
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

// 该对象是否参与求解器物理：
// - Mario（玩家角色）：由自己的事件物理控制（落地清零/撞墙手感），求解器介入会
//   把输入驱动的水平速度也清掉（跳跃擦到水平物体边缘时动不了）；
// - Box（问号箱）：自身有 last_y 复位逻辑，不参与碰撞物理，求解器介入会导致"慢慢上天"；
// - FireBall（火球）：有特殊的弹跳逻辑（保证最低弹跳速度 fireballSpeedY + 水平碰撞爆炸），
//   通用求解器会破坏"一定能弹起来"的游戏设计。
// 这些对象在 Contact 中仍会推动对方（对方正常响应），只是自己不被求解器动。
bool isSolverExcluded(const std::shared_ptr<GameObject>& obj) {
    const std::string& cls = obj->getClassName();
    return cls == "Mario" || cls == "Box" || cls == "FireBall";
}

void applyImpulse(const std::shared_ptr<GameObject>& obj, const sf::Vector2f& dv) {
    if (!obj->getMoveAble()) return;
    if (isSolverExcluded(obj)) return;
    if (const auto mc = obj->getComponent<MoveComponent>(); mc && mc->getActive()) {
        mc->addSpeed(dv);
    }
}

void applyCorrection(const std::shared_ptr<GameObject>& obj, const sf::Vector2f& dp) {
    if (!obj->getMoveAble()) return;
    if (isSolverExcluded(obj)) return;
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

    // 第一阶段：检测 → Contact 列表（几何基于帧开始时的快照，与事件快照一致）
    std::vector<Contact> contacts;

    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t j = i + 1; j < objects.size(); j++) {
            const auto a = objects[i];
            const auto b = objects[j];
            if (!a->getMoveAble() && !b->getMoveAble()) continue;
            if (!a->isActive() || !b->isActive()) continue;

            const auto a_c = a->getComponent<Collision>();
            if (!a_c || !a_c->getActive()) continue;

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
        }
    }

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

void CollisionSystem::solveContacts(std::vector<Contact>& contacts) {
    // 速度迭代：多轮收敛，每轮使用最新速度，让相互耦合的接触（堆叠）逐步逼近一致解，
    // 消除"处理顺序依赖 + 快照过期"导致的振荡
    for (int iter = 0; iter < SOLVER_ITERATIONS; iter++) {
        for (auto& c : contacts) {
            const sf::Vector2f va = c.a->getSpeed();
            const sf::Vector2f vb = c.b->getSpeed();
            const float v_rel_n = (va.x - vb.x) * c.normal.x + (va.y - vb.y) * c.normal.y;
            if (v_rel_n >= 0.f) continue;  // 正在分离或相对静止，无需处理

            if (-v_rel_n < RESTING_SPEED_THRESHOLD) {
                // 接触静止：清除双方法向速度，不再注入回弹（避免持续注能微弹跳）
                const float va_n = va.x * c.normal.x + va.y * c.normal.y;
                const float vb_n = vb.x * c.normal.x + vb.y * c.normal.y;
                applyImpulse(c.a, -c.normal * va_n);
                applyImpulse(c.b, -c.normal * vb_n);
            } else {
                // 快速接近：注入分离冲量（回弹）。
                // 恢复系数模型：分离速度 = e × 接近速度，冲量 = -(1+e) × v_rel_n / (invMa+invMb)。
                // 质量相等假设下 invMa+invMb = 2（双方可动）或 1（一方静止，其 invMass=0）。
                // 注意：不除分母会导致双方可动时实际恢复系数变成 1+2e（超弹性，越碰越快）。
                const bool a_mov = c.a->getMoveAble();
                const bool b_mov = c.b->getMoveAble();
                const float restitution = (a_mov && b_mov) ? BOUNCE_FACTOR_MOVING : BOUNCE_FACTOR_STATIC;
                const float inv_mass_sum = (a_mov && b_mov) ? 2.f : 1.f;
                const float impulse = -(1.f + restitution) * v_rel_n / inv_mass_sum;
                applyImpulse(c.a, c.normal * impulse);
                applyImpulse(c.b, -c.normal * impulse);
            }
        }
    }

    // 位置修正：一次全量分摊（双方可动各推一半，静止方不动）。
    // 在速度迭代之后做一次，避免多轮迭代累计 overshoot。
    for (auto& c : contacts) {
        const bool a_mov = c.a->getMoveAble();
        const bool b_mov = c.b->getMoveAble();
        if (a_mov && b_mov) {
            applyCorrection(c.a, c.normal * c.penetration * 0.5f);
            applyCorrection(c.b, -c.normal * c.penetration * 0.5f);
        } else if (a_mov) {
            applyCorrection(c.a, c.normal * c.penetration);
        } else if (b_mov) {
            applyCorrection(c.b, -c.normal * c.penetration);
        }
    }
}

std::vector<std::shared_ptr<GameObject>>* CollisionSystem::getObjects() {
    return &objects;
}
