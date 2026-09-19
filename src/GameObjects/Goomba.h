//
// Created by MINEC on 2026/9/15.
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

class Goomba : public NetworkGameObject {
public:
    Goomba(float x, float y, float speed_x = -100.f);

    ~Goomba() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 被马里奥踩扁：关闭碰撞与运动，延时销毁
    void setSquashed();

    // 被炮弹击毙：沿炮弹方向炸飞并坠落穿出场景（blast_dir_x 为炸飞方向 ±1）
    void setKilledByFireball(float blast_dir_x);

    // 是否已死亡（被踩扁或被炮弹击毙）
    bool isDead() const {
        return is_squashed;
    }

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

    // 服务端销毁时广播 RemoveObject，客户端本地销毁静默（方案 B：移除由服务端统一裁决）
    void destroy() override;

private:
    bool is_squashed = false;
    bool killed_by_fireball = false;
#ifndef SERVER_BUILD
    Animation walkAnimation;
    Animation squashAnimation;
    MIX_Track* stomp_track = nullptr;   // 踩扁音效
    MIX_Track* kick_track = nullptr;    // 被炸飞音效
#endif
    Timer squash_timer;
};
