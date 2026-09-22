//
// Created by MINEC on 2026/9/17.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"

// 乌龟大王的旋转飞斧：抛物线轨迹 + 高速旋转贴图；
// 命中马里奥的伤害与销毁由马里奥侧的 handleCollision 结算，对小怪与 BOSS 本体穿行。
// 联机：由服务端权威生成并广播，客户端为提线木偶（快照硬设位置，
// 生命周期由服务端 RemoveObject 驱动，本地不做 TTL 预判）
class BowserAxe : public NetworkGameObject {
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

    // ISerializable：SpawnObject 快照重建，UpdateObject 供客户端硬同步
    void serialize(eng::Packet& packet, NetworkMsg type) override;
    void deserialize(eng::Packet& packet) override;

    void destroy() override;

private:
    // 本端是否为该对象的模拟权威（专用服务器/桌面主机/单机；纯客户端不算）
    bool isAuthority() const;

    bool is_destroyed = false;
#ifndef SERVER_BUILD
    Animation animation;
#endif
    Timer ttl_timer;   // 仅权威端启动（存活上限）
};
