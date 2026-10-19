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

    // play_sound：本地按键时播放音效（服务端构建里无音效）
    // allow_buffer：网络来的起跳请求允许短暂缓冲（见 update 里的起跳缓冲）
    void jump(bool play_sound = true, bool allow_buffer = false);

    void runLeft() const;

    void runRight() const;

    void stopRun() const;

    void setIsPlayer(bool flag);

    void setWisPressed(bool flag);

    void shoot(bool play_sound = true);

private:
    // 此刻能不能起跳：只看物理（踩在实体上、且没在上升），不看状态机名字
    bool canJumpNow();

#ifndef SERVER_BUILD
    // 一次性音效 = 常驻 track（绑预解码 MIX_Audio，play 时 restart）
    MIX_Track* jump_track = nullptr;
    MIX_Track* big_jump_track = nullptr;   // 大马里奥跳跃音效（与小的不同）
    MIX_Track* shoot_track = nullptr;
#endif
    bool w_is_pressed = false;
    bool a_held = false;    // A 键按住状态（供松键时判断另一方向是否仍按住）
    bool d_held = false;    // D 键按住状态（供松键时判断另一方向是否仍按住）
    Timer jump_timer;
    // 起跳缓冲：服务端收到起跳输入时可能只差一两帧才落地，先存着、落地立刻补跳。
    // 不做这个缓冲，服务端会把这次起跳丢掉，而客户端本地已经跳了，
    // 随后到达的服务端快照会把角色强行拉回地面。
    bool jump_buffered = false;
    bool jump_buffer_play_sound = false;
    Timer jump_buffer_timer;
    bool could_shoot = true;
    Timer shoot_timer;
    bool is_player = true;
};

