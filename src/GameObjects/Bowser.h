//
// Created by MINEC on 2026/9/17.
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

// 乌龟大王 BOSS：AI 决策驱动。平时沉睡隐身，马里奥接近到阈值内才现身，
// 之后按决策心跳（1.5s）依与马里奥的距离加权随机执行：巡逻/站定张嘴喷火/
// 朝马里奥跳扑/掷出旋转飞斧。碰撞盒小于渲染贴图（水平居中、底部对齐）。
// 5 点血量，被马里奥炮弹打空后炸飞坠落（龟壳踩不得，接触即伤由马里奥侧结算）
// 联机：AI 用非确定性随机源，无法双端一致模拟——服务端（含单机）权威模拟，
// 客户端为提线木偶（快照硬设位置/状态，不跑 AI、不扣血、不生成投射物）
class Bowser : public NetworkGameObject {
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

    // ISerializable：SpawnObject 快照支持中途加入还原，UpdateObject 供客户端硬同步
    void serialize(eng::Packet& packet, NetworkMsg type) override;
    void deserialize(eng::Packet& packet) override;

    // 新客户端中途加入：按 SpawnObject 快照静默还原（不重放激活/死亡动画与音效，
    // 参照 Mushroom::restoreNetworkState）
    void restoreNetworkState(bool activated, bool breathing, int health, bool killed);

    void destroy() override;

private:
    // 本端是否为该对象的模拟权威（专用服务器/桌面主机/单机；纯客户端不算）
    bool isAuthority() const;

    // 投射物生成：联机（主机/服务器）走 addObjectWithNetwork 注册并广播 SpawnObject；
    // 单机（None）本地直接生成（Box 顶砖生成同款判定）
    void spawnProjectile(Scene* scene, const std::shared_ptr<GameObject>& obj) const;

#ifndef SERVER_BUILD
    // 朝向变化时切换走路/喷火两组动画的帧集（update 与 deserialize 共用）
    void switchFacing();
#endif

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
    Timer activate_timer;            // 延迟激活检查（循环计时，仅权威端启动）
    Timer decision_timer;            // AI 决策心跳（循环计时）
    Timer axe_timer;                 // 飞斧连发间隔计时（一次性，连发期间逐发触发）
    Timer shoot_timer;               // 张嘴后延时吐火焰弹（一次性）
    Timer breath_timer;              // 喷火动作总时长（一次性）
    int axe_burst_left = 0;          // 本轮连发剩余斧数
};
