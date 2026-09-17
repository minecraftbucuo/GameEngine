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

// 乌龟大王 BOSS：AI 决策驱动。平时沉睡隐身，马里奥接近到阈值内才现身，
// 之后按决策心跳（1.5s）依与马里奥的距离加权随机执行：巡逻/站定张嘴喷火/
// 朝马里奥跳扑/掷出旋转飞斧。碰撞盒小于渲染贴图（水平居中、底部对齐）。
// 3 点血量，被马里奥炮弹打空后炸飞坠落（龟壳踩不得，接触即伤由马里奥侧结算）
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
    // AI 决策心跳：按与马里奥的距离加权随机选择动作（喷火中/腾空不决策）
    void makeDecision();
    // 跳扑：跳起并朝马里奥方向前压，落地由垂直碰撞恢复巡逻
    void jumpAction();
    // 掷斧：从头顶向马里奥方向连发 2~3 把旋转飞斧
    void throwAxe();
    // 连发中的单发：发一把并按需安排下一发
    void throwAxeOne();
    // 延迟激活：马里奥接近到阈值内才现身开始行动
    void checkActivation();

    // 进入喷火状态：站定 → 播张嘴动画 → 延时吐出火焰弹 → 收嘴恢复巡逻
    void startBreathing();
    void stopBreathing();
    void spawnFire();

    bool is_killed = false;
    bool is_breathing = false;
    bool is_activated = false;       // 延迟激活：马里奥接近前保持沉睡隐身
    bool is_in_air = false;          // 跳扑腾空标志（落地清除，腾空中不决策）
    bool facing_left = true;
#ifndef SERVER_BUILD
    bool anim_facing_left = true;    // 当前动画帧集对应的朝向（避免每帧重设帧集）
    Animation walkAnimation;         // 走路动画（按朝向切换帧集）
    Animation breathAnimation;       // 张嘴喷火动画（按朝向切换帧集）
    MIX_Track* kick_track = nullptr; // 被击败音效
#endif
    float patrol_speed_x = 80.f;     // 巡逻速度绝对值（动作结束后恢复）
    Timer activate_timer;            // 延迟激活检查（循环计时）
    Timer decision_timer;            // AI 决策心跳（循环计时）
    Timer axe_timer;                 // 飞斧连发间隔计时（一次性，连发期间逐发触发）
    Timer shoot_timer;               // 张嘴后延时吐火焰弹（一次性）
    Timer breath_timer;              // 喷火动作总时长（一次性）
    int axe_burst_left = 0;          // 本轮连发剩余斧数
};
