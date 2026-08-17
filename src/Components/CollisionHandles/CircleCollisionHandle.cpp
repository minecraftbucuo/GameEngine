//
// Created by MINEC on 2026/5/8.
//

#include "CircleCollisionHandle.h"

#include "BoxCollision.h"
#include <cmath>
#include "GameObject.h"
#include "MoveComponent.h"

namespace {
// 接触静止判定的接近速度阈值：法向接近速度低于该值视为静止接触，
// 不再注入回弹冲量（避免重力+冲量持续注能导致的微弹跳）。
// 调大后堆叠振荡速度（冲击在堆内来回反射的速度）也能被快速吸收，减少"弹簧感"。
// 阶段 B 再配置化。
constexpr float RESTING_SPEED_THRESHOLD = 40.f;
// 球-球回弹系数：调小以抑制堆叠振荡（新球砸在堆顶时向堆内注入的能量）。
// 注意球-地面弹跳走 box 路径（CollisionHandle.cpp 的 0.28），不受此值影响。
// 阶段 B 换恢复系数模型。
constexpr float BOUNCE_FACTOR = 0.1f;
}

CircleCollisionHandle::CircleCollisionHandle() {
    collisionHandlers[typeid(BoxCollision).hash_code()] = [this](const CollisionEvent& event) {
        this->handleCollisionWithBox(event);
    };
    collisionHandlers[typeid(CircleCollision).hash_code()] = [this](const CollisionEvent& event) {
        this->handleCollisionWithCircle(event);
    };
}

void CircleCollisionHandle::handleCollisionWithBox(const CollisionEvent& event) {
    handle(event);
}

void CircleCollisionHandle::handleCollisionWithCircle(const CollisionEvent& event) {
    auto &this_ = event.a;
    auto &other = event.b;

    if (!this_->getMoveAble()) return;
    const std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
    if (!moveComponent) return;

    const sf::Vector2f center_a = event.a_position + this_->getSize() * 0.5f;
    const sf::Vector2f center_b = event.b_position + other->getSize() * 0.5f;

    sf::Vector2f dir = center_a - center_b;
    const float dir_len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    // 除零保护：两圆圆心精确重合（dir_len≈0，如点击生成在同一坐标）时 dir 无法归一化，
    // 直接 `dir /= dir_len` 会得到 NaN，NaN 速度/位置会让球瞬移/乱跳。
    // 此时取一个安全的分离方向，且跳过冲量注入，只做位置分离。
    const bool degenerate = dir_len < 1e-6f;
    if (degenerate) {
        // 能分辨符号时按位置差取轴对齐方向（优先水平，球叠球时水平推开更不易再次纠缠）；
        // 完全重合时固定向上
        if (std::abs(dir.x) > 1e-6f) {
            dir = sf::Vector2f(dir.x > 0.f ? 1.f : -1.f, 0.f);
        } else if (std::abs(dir.y) > 1e-6f) {
            dir = sf::Vector2f(0.f, dir.y > 0.f ? 1.f : -1.f);
        } else {
            dir = sf::Vector2f(0.f, -1.f);
        }
    } else {
        dir /= dir_len;
    }

    const sf::Vector2f relativeSpeed = event.a_speed - event.b_speed;

    // 冲量：沿法向把本方推离对方（双方各自的事件都会执行，碰撞是相互的）
    if (!degenerate) {
        // 法向相对速度（dir 从 b 指向 a；v_rel_n < 0 表示正在接近）
        const float v_rel_n = relativeSpeed.x * dir.x + relativeSpeed.y * dir.y;
        if (v_rel_n < 0.f && -v_rel_n < RESTING_SPEED_THRESHOLD) {
            // 接触静止：把本方法向速度清零，不再注入回弹冲量。
            // 否则重力每帧累加速度 + 每次接触注入 0.28×回弹，叠球永远微弹跳甚至累积成大跳。
            const float speed_n = event.a_speed.x * dir.x + event.a_speed.y * dir.y;
            moveComponent->addSpeed(-dir * speed_n);
        } else {
            // 接近速度较大：保留回弹冲量（阶段 B 换恢复系数模型）。
            // BOUNCE_FACTOR 调小：新球砸到堆顶时向堆内注入的能量少，堆叠不易振荡。
            const float relativeSpeed_len = std::sqrt(
                relativeSpeed.x * relativeSpeed.x + relativeSpeed.y * relativeSpeed.y);
            moveComponent->addSpeed(dir * relativeSpeed_len * BOUNCE_FACTOR);
        }
    }

    // 位置分离：双方各分摊一半修正距离，避免双方各自移动完整重叠距离导致总分离 2 倍。
    // 若对方不可移动（如地面），则由本方推完整距离（对方不动）。
    // 全量修正：一帧内完全分开（收敛最快）。曾尝试比例修正/单帧限幅，实测收敛偏慢被否决。
    const float penetration = this_->getSize().x / 2 + other->getSize().x / 2 - dir_len;
    if (other->getMoveAble()) {
        moveComponent->addPosition(penetration * 0.5f * dir);
    } else {
        moveComponent->addPosition(penetration * dir);
    }
}
