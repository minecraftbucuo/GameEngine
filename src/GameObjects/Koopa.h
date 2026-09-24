//
// Created by MINEC on 2026/9/24.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"

class Scene;

#ifndef SERVER_BUILD
struct MIX_Track;   // SDL_mixer track 前置声明（头文件不引 SDL 头）
#endif

// 三态状态机（线上格式按 uint8_t 序列化，枚举值即协议值）
enum class KoopaState : uint8_t {
    Walking = 0,     // 行走巡逻
    ShellIdle = 1,   // 静止龟壳（无伤，可被踢）
    ShellMoving = 2  // 滑动龟壳（撞敌人致命，侧碰马里奥受伤）
};

// 乌龟（Koopa Troopa）：单类三态状态机，不做独立龟壳类——状态转换频繁且可逆
//（踢→踩停→复活→再踢），独立类每次转换都要销毁+重建换网络 id，与"服务端是
// 移除唯一权威"的销毁协议冲突；Goomba 踩扁即同类内状态先例。
//
// 状态转换：
//   Walking    被踩 → ShellIdle（马里奥反弹）；侧碰 → 马里奥受伤、乌龟掉头
//   ShellIdle  被踩/侧碰 → ShellMoving（方向 = 马里奥指向壳的相反方向）；
//              静置 5s（权威端计时）→ Walking 复活
//   ShellMoving被踩 → ShellIdle（踩停）；侧碰 → 马里奥受伤（踢壳豁免期除外）；
//              撞墙 → 反向继续滑动；撞到 Goomba/Mushroom → 由受害者侧结算弹飞
//
// 联机：乌龟 AI（恒速巡逻/撞墙掉头）与壳的滑动均为确定性模拟，采用 Goomba 的
// 方案 B——客户端预测交互结果后经 ClientEvent 上报（KoopaStomped/KoopaKicked），
// 服务端幂等方法裁决；128Hz 快照同步位置/速度/状态，状态差异走幂等补流程。
class Koopa : public NetworkGameObject {
public:
    Koopa(float x, float y, float speed_x = -100.f);

    ~Koopa() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void handleCollision(const CollisionEvent& event);

    // 被踩（Walking→ShellIdle）或滑动壳被踩停（ShellMoving→ShellIdle）：
    // 幂等守卫；客户端预测成功后上报 KoopaStomped；权威端启动 5s 复活计时
    void setShellIdle();

    // 踢出静止壳（仅 ShellIdle 生效，幂等）：沿 dir_x 方向滑动；客户端上报
    // KoopaKicked(dir)；启动 250ms 踢壳豁免（双端确定性，豁免期内马里奥侧碰不受伤）
    void kicked(float dir_x);

    // 被炮弹/BOSS 火焰弹击毙（幂等）：关碰撞保留重力，沿炸飞方向抛物线坠落，
    // 掉出场景由 update 销毁；客户端预测后上报 KoopaKilledByFireball(dir)
    void setKilledByFireball(float dir_x);

    // 静止壳复活为行走乌龟（仅 ShellIdle 生效，幂等）
    void revive();

    // 行走中被马里奥侧碰掉头（马里奥侧调用）
    void reverse();

    bool isShellMoving() const {
        return state == KoopaState::ShellMoving;
    }

    // 踢壳豁免期内：滑动壳侧碰马里奥不结算受伤（壳刚被踢出时马里奥仍与其重叠）
    bool isKickGraceActive() const;

    // 变壳豁免期内：刚缩成/踩停成静止壳时马里奥的接触不触发踢出
    //（否则踩中变壳的下一帧，仍压在壳上的马里奥会立刻把它踢走，壳无法保持静止）
    bool isShellGraceActive() const;

    KoopaState getState() const {
        return state;
    }

    // ISerializable：SpawnObject 快照支持中途加入还原，UpdateObject 供客户端同步
    void serialize(eng::Packet& packet, NetworkMsg type) override;
    void deserialize(eng::Packet& packet) override;

    // 新客户端中途加入：按 SpawnObject 快照静默还原（快照 y 即当前态权威左上角，
    // 不做状态转换的 y 平移、不重放音效/计时）
    void restoreNetworkState(KoopaState state, bool facing_left, float speed_x);

    // 服务端销毁时广播 RemoveObject，客户端本地销毁静默（方案 B：移除由服务端统一裁决）
    void destroy() override;

private:
    // 本端是否为该对象的模拟权威（专用服务器/桌面主机/单机；纯客户端不算）
    bool isAuthority() const;

#ifndef SERVER_BUILD
    // 朝向变化时切换左/右行走帧集（render 与 deserialize 共用）
    void switchFacing();
#endif

    // 统一尺寸重建：状态转换保持底边对齐（左上角锚点 y 平移），行走 64×96、壳 64×64
    void applyStateSize(KoopaState new_state);

    KoopaState state = KoopaState::Walking;
    bool is_killed = false;          // 被火系击毙：坠落中，不再参与任何交互
    bool facing_left = true;
    float patrol_speed_x = -100.f;   // 巡逻速度（带符号，复活的恢复值）
#ifndef SERVER_BUILD
    bool anim_facing_left = true;    // 当前动画帧集对应的朝向（避免每帧重设帧集）
    Animation walkAnimation;         // 行走动画（按朝向切换左/右帧集）
    Animation shellAnimation;        // 龟壳（静止帧）
    MIX_Track* stomp_track = nullptr;   // 踩缩壳/踩停音效
    MIX_Track* kick_track = nullptr;    // 踢壳音效
#endif
    Timer revive_timer;              // 静止壳复活计时（一次性，仅权威端更新）
    Timer kick_grace_timer;          // 踢壳豁免计时（一次性，双端确定性更新）
    Timer stomp_grace_timer;         // 变壳豁免计时（一次性，双端确定性更新）
};
