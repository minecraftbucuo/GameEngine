//
// Created by MINEC on 2026/9/17.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "GameObject.h"
#include "Timer.h"
#include "Core/Types.h"

// 乌龟大王的旋转飞斧：抛物线轨迹 + 高速旋转贴图；
// 命中马里奥的伤害与销毁由马里奥侧的 handleCollision 结算，对小怪与 BOSS 本体穿行
class BowserAxe : public GameObject {
public:
    BowserAxe(float x, float y, float speed_x);

    ~BowserAxe() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 命中目标/撞墙/超时：立即销毁
    void setDestroyed();

private:
    bool is_destroyed = false;
#ifndef SERVER_BUILD
    Animation animation;
#endif
    Timer ttl_timer;
};
