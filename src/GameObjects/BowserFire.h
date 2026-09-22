//
// Created by MINEC on 2026/9/17.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"

// 乌龟大王的火焰弹：直线飞行（无重力），只被地形/可顶物阻挡熄灭，
// 穿过其他对象（含友军）；命中马里奥的伤害与熄灭由马里奥侧的 handleCollision 结算。
// 联机：由服务端权威生成并广播，客户端为提线木偶（快照硬设位置，
// 生命周期由服务端 RemoveObject 驱动，本地不做 TTL 预判）
class BowserFire : public NetworkGameObject {
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

    // ISerializable：SpawnObject 快照重建，UpdateObject 供客户端硬同步
    void serialize(eng::Packet& packet, NetworkMsg type) override;
    void deserialize(eng::Packet& packet) override;

    void destroy() override;

private:
    // 本端是否为该对象的模拟权威（专用服务器/桌面主机/单机；纯客户端不算）
    bool isAuthority() const;

    bool is_extinguished = false;
#ifndef SERVER_BUILD
    Animation animation;
#endif
    Timer ttl_timer;   // 仅权威端启动（存活上限）
};
