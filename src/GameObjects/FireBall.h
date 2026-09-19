//
// Created by MINEC on 2026/5/7.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"


class FireBall : public NetworkGameObject {
public:
    FireBall(unsigned int owner_id, float x, float y, float speed_x = 0.f);

    ~FireBall() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void setExploded();

    void handleCollision(const CollisionEvent& event);

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

    // 服务端销毁时广播 RemoveObject（客户端本地销毁静默，方案 B：移除由服务端统一裁决）
    void destroy() override;

    unsigned int getOwnerId() const;

private:
    bool is_exploded = false;
#ifndef SERVER_BUILD
    Animation animation;
    Animation explosionAnimation;
#endif
    unsigned int owner_id;
    Timer ttl_timer;
    // 服务端爆炸后延迟销毁：给客户端留出播放爆炸动画的窗口（立即 RemoveObject 会截断动画）
    Timer explode_timer;
};
