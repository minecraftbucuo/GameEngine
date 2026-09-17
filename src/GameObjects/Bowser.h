//
// Created by MINEC on 2026/9/17.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "GameObject.h"
#include "Timer.h"
#include "Core/Types.h"

#ifndef SERVER_BUILD
struct MIX_Track;   // SDL_mixer track 前置声明（头文件不引 SDL 头）
#endif

// 乌龟大王 BOSS：地面巡逻，定时站定张嘴喷吐火焰弹；3 点血量，
// 被马里奥炮弹打空后炸飞坠落（龟壳踩不得，接触即伤由马里奥侧结算）
class Bowser : public GameObject {
public:
    Bowser(float x, float y, float speed_x = -80.f);

    ~Bowser() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 血量打空被击败：沿炮弹方向炸飞并坠落穿出场景（blast_dir_x 为炸飞方向 ±1）
    void setKilled(float blast_dir_x);

    bool isDead() const {
        return is_killed;
    }

private:
    // 进入喷火状态：站定 → 播张嘴动画 → 延时吐出火焰弹 → 收嘴恢复巡逻
    void startBreathing();
    void stopBreathing();
    void spawnFire();

    bool is_killed = false;
    bool is_breathing = false;
    bool facing_left = true;
#ifndef SERVER_BUILD
    bool anim_facing_left = true;    // 当前动画帧集对应的朝向（避免每帧重设帧集）
    Animation walkAnimation;         // 走路动画（按朝向切换帧集）
    Animation breathAnimation;       // 张嘴喷火动画（按朝向切换帧集）
    MIX_Track* fire_track = nullptr; // 喷火音效
    MIX_Track* kick_track = nullptr; // 被击败音效
#endif
    float patrol_speed_x = 80.f;     // 巡逻速度绝对值（喷火收嘴后恢复）
    Timer fire_timer;                // 喷火冷却（循环计时）
    Timer shoot_timer;               // 张嘴后延时吐火焰弹（一次性）
    Timer breath_timer;              // 喷火动作总时长（一次性）
};
