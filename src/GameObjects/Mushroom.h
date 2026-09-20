//
// Created by MINEC on 2026/9/18.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"

#ifndef SERVER_BUILD
struct MIX_Track;   // SDL_mixer track 前置声明（头文件不引 SDL 头）
#endif

class Mushroom : public NetworkGameObject {
public:
    Mushroom(float x, float y, float speed_x = -100.f);

    ~Mushroom() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 被马里奥吃掉：关闭运动与碰撞，播放获得音效后延时销毁
    void setEaten();

    // 被敌人/BOSS/飞斧击杀：向远离敌人的方向弹飞，坠落穿出场景后销毁
    void setKilled(float blast_dir_x);

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

    // 按服务端快照还原已有蘑菇的状态：已长出/已被吃的蘑菇不重放升起动画与出生音效
    void restoreNetworkState(float birth_y, bool emerging, bool eaten);

    // 服务端销毁时广播 RemoveObject，客户端本地销毁静默（方案 B：移除由服务端统一裁决）
    void destroy() override;

private:
    // 升起阶段：从方块内部匀速上移一个方块位，期间不响应碰撞、不受重力
    // （碰撞盒保持开启，马里奥升起途中即可吃掉；重力升起结束后再开）
    bool is_emerging = true;
    bool is_eaten = false;
    float spawn_y = 0.f;
    float walk_speed = -100.f;
#ifndef SERVER_BUILD
    Animation idleAnimation;
    MIX_Track* appear_track = nullptr;   // 从方块里长出来的音效
    MIX_Track* eaten_track = nullptr;    // 被吃掉的音效
#endif
    Timer eaten_timer;
};
