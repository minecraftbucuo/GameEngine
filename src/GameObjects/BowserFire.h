//
// Created by MINEC on 2026/9/17.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "GameObject.h"
#include "Timer.h"
#include "Core/Types.h"

// 乌龟大王的火焰弹：直线飞行（无重力），只被地形/可顶物阻挡熄灭，
// 穿过其他对象（含友军）；命中马里奥的伤害与熄灭由马里奥侧的 handleCollision 结算
class BowserFire : public GameObject {
public:
    BowserFire(float x, float y, float speed_x);

    ~BowserFire() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 熄灭（撞墙/命中目标/超时）：立即销毁
    void setExtinguished();

private:
    bool is_extinguished = false;
#ifndef SERVER_BUILD
    Animation animation;
#endif
    Timer ttl_timer;
};
