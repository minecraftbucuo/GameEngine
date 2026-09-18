//
// Created by MINEC on 2026/9/18.
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

class Mushroom : public GameObject {
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

private:
    // 升起阶段：从方块内部匀速上移一个方块位，期间不参与碰撞、不受重力
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
