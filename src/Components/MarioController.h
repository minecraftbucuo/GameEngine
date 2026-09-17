//
// Created by MINEC on 2026/2/2.
//

#pragma once

#include "Component.h"
#include "Timer.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
struct MIX_Track;   // SDL_mixer track 前置声明（头文件不引 SDL 头）
#endif

class MarioController : public Component {
public:
    ~MarioController() override;

    void start() override;

    void handleEvent(const eng::EngineEvent& event) override;

    void update(const eng::Time& deltaTime) override;

    void jump(bool play_sound = true);

    void runLeft() const;

    void runRight() const;

    void stopRun() const;

    void setIsPlayer(bool flag);

    void setWisPressed(bool flag);

    void shoot(bool play_sound = true);

    // 当前按住的水平方向：+1 右、-1 左、0 未按（双键同按视为 0）
    int heldDirectionX() const { return d_held && !a_held ? 1 : (a_held && !d_held ? -1 : 0); }

private:
#ifndef SERVER_BUILD
    // 一次性音效 = 常驻 track（绑预解码 MIX_Audio，play 时 restart）
    MIX_Track* jump_track = nullptr;
    MIX_Track* shoot_track = nullptr;
#endif
    bool w_is_pressed = false;
    bool a_held = false;    // A 键按住状态（供每帧方向纠偏）
    bool d_held = false;    // D 键按住状态（供每帧方向纠偏）
    Timer jump_timer;
    bool could_shoot = true;
    Timer shoot_timer;
    bool is_player = true;
};

